/**
 * Admin Dashboard Core JavaScript
 * WaitingTheLongest.com
 * Author: Agent 20
 *
 * Core functionality for the admin dashboard including:
 * - API communication
 * - Authentication
 * - Navigation
 * - Modals and toasts
 * - WebSocket connection
 */

// ============================================================================
// CONFIGURATION
// ============================================================================

const AdminConfig = {
    apiBaseUrl: '/api/admin',
    authBaseUrl: '/api/auth',
    wsBaseUrl: `ws://${window.location.host}/ws`,
    refreshInterval: 60000,  // 1 minute
    toastDuration: 5000,     // 5 seconds
    pageSize: 20,
    maxPageSize: 100
};

// ============================================================================
// STATE
// ============================================================================

const AdminState = {
    currentPage: 'dashboard',
    authToken: null,
    user: null,
    websocket: null,
    isConnected: false,
    refreshTimers: {},
    cache: {}
};

// ============================================================================
// API CLIENT
// ============================================================================

const AdminAPI = {
    /**
     * Make an authenticated API request
     */
    async request(endpoint, options = {}) {
        const url = endpoint.startsWith('http') ? endpoint : `${AdminConfig.apiBaseUrl}${endpoint}`;

        const headers = {
            'Content-Type': 'application/json',
            ...options.headers
        };

        if (AdminState.authToken) {
            headers['Authorization'] = `Bearer ${AdminState.authToken}`;
        }

        try {
            const response = await fetch(url, {
                ...options,
                headers
            });

            const data = await response.json();

            if (!response.ok) {
                if (response.status === 401) {
                    AdminAuth.handleUnauthorized();
                }
                throw new Error(data.error?.message || 'Request failed');
            }

            return data;
        } catch (error) {
            console.error('API Error:', error);
            throw error;
        }
    },

    /**
     * GET request
     */
    async get(endpoint, params = {}) {
        const queryString = new URLSearchParams(params).toString();
        const url = queryString ? `${endpoint}?${queryString}` : endpoint;
        return this.request(url, { method: 'GET' });
    },

    /**
     * POST request
     */
    async post(endpoint, data = {}) {
        return this.request(endpoint, {
            method: 'POST',
            body: JSON.stringify(data)
        });
    },

    /**
     * PUT request
     */
    async put(endpoint, data = {}) {
        return this.request(endpoint, {
            method: 'PUT',
            body: JSON.stringify(data)
        });
    },

    /**
     * DELETE request
     */
    async delete(endpoint) {
        return this.request(endpoint, { method: 'DELETE' });
    }
};

// ============================================================================
// AUTHENTICATION
// ============================================================================

const AdminAuth = {
    /**
     * Initialize authentication
     */
    async init() {
        const token = localStorage.getItem('admin_token');
        if (token) {
            AdminState.authToken = token;
            try {
                const response = await AdminAPI.get('/api/auth/me'.replace('/api/admin', ''));
                AdminState.user = response.data;
                this.updateUserInfo();
                return true;
            } catch (error) {
                this.logout();
                return false;
            }
        }
        return false;
    },

    /**
     * Login
     */
    async login(email, password) {
        try {
            const response = await fetch(`${AdminConfig.authBaseUrl}/login`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ email, password })
            });

            const data = await response.json();

            if (!response.ok) {
                throw new Error(data.error?.message || 'Login failed');
            }

            AdminState.authToken = data.data.access_token;
            AdminState.user = data.data.user;

            localStorage.setItem('admin_token', AdminState.authToken);
            this.updateUserInfo();

            return true;
        } catch (error) {
            AdminToast.error('Login Failed', error.message);
            return false;
        }
    },

    /**
     * Logout
     */
    logout() {
        AdminState.authToken = null;
        AdminState.user = null;
        localStorage.removeItem('admin_token');
        window.location.href = '/login';
    },

    /**
     * Handle unauthorized response
     */
    handleUnauthorized() {
        AdminToast.warning('Session Expired', 'Please log in again');
        this.logout();
    },

    /**
     * Update user info display
     */
    updateUserInfo() {
        if (AdminState.user) {
            const nameEl = document.getElementById('admin-name');
            if (nameEl) {
                nameEl.textContent = AdminState.user.display_name || AdminState.user.email;
            }
        }
    }
};

// ============================================================================
// NAVIGATION
// ============================================================================

const AdminNav = {
    /**
     * Initialize navigation
     */
    init() {
        this.bindEvents();
        this.handleHashChange();
        window.addEventListener('hashchange', () => this.handleHashChange());
    },

    /**
     * Bind navigation events
     */
    bindEvents() {
        // Sidebar toggle
        const sidebarToggle = document.getElementById('sidebar-toggle');
        if (sidebarToggle) {
            sidebarToggle.addEventListener('click', () => this.toggleSidebar());
        }

        // Mobile menu toggle
        const mobileToggle = document.getElementById('mobile-menu-toggle');
        if (mobileToggle) {
            mobileToggle.addEventListener('click', () => this.toggleMobileSidebar());
        }

        // Nav links
        document.querySelectorAll('.nav-item').forEach(item => {
            item.addEventListener('click', (e) => {
                const page = item.dataset.page;
                if (page) {
                    e.preventDefault();
                    this.navigateTo(page);
                }
            });
        });

        // Logout button
        const logoutBtn = document.getElementById('logout-btn');
        if (logoutBtn) {
            logoutBtn.addEventListener('click', () => AdminAuth.logout());
        }

        // Refresh button
        const refreshBtn = document.getElementById('refresh-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.refreshCurrentPage());
        }
    },

    /**
     * Handle hash change
     */
    handleHashChange() {
        const hash = window.location.hash.replace('#', '') || 'dashboard';
        this.navigateTo(hash, false);
    },

    /**
     * Navigate to a page
     */
    navigateTo(page, updateHash = true) {
        // Update active nav item
        document.querySelectorAll('.nav-item').forEach(item => {
            item.classList.toggle('active', item.dataset.page === page);
        });

        // Update page title
        const titles = {
            dashboard: 'Dashboard',
            dogs: 'Dog Management',
            shelters: 'Shelter Management',
            users: 'User Management',
            fosters: 'Foster Management',
            content: 'Content Management',
            analytics: 'Analytics',
            system: 'System Health',
            config: 'Configuration',
            audit: 'Audit Log'
        };

        const titleEl = document.getElementById('page-title');
        if (titleEl) {
            titleEl.textContent = titles[page] || page;
        }

        // Show/hide pages
        document.querySelectorAll('.page').forEach(el => {
            el.classList.toggle('active', el.id === `page-${page}`);
        });

        // Update hash
        if (updateHash) {
            window.location.hash = page;
        }

        // Update state
        AdminState.currentPage = page;

        // Load page data
        this.loadPageData(page);

        // Close mobile sidebar
        document.getElementById('sidebar')?.classList.remove('open');
    },

    /**
     * Load page-specific data
     */
    loadPageData(page) {
        switch (page) {
            case 'dashboard':
                if (typeof AdminDashboard !== 'undefined') {
                    AdminDashboard.load();
                }
                break;
            case 'dogs':
                if (typeof DogsManager !== 'undefined') {
                    DogsManager.load();
                }
                break;
            case 'shelters':
                if (typeof SheltersManager !== 'undefined') {
                    SheltersManager.load();
                }
                break;
            case 'users':
                if (typeof UsersManager !== 'undefined') {
                    UsersManager.load();
                }
                break;
            case 'content':
                if (typeof ContentManager !== 'undefined') {
                    ContentManager.load();
                }
                break;
            case 'analytics':
                if (typeof AnalyticsDashboard !== 'undefined') {
                    AnalyticsDashboard.load();
                }
                break;
            case 'system':
                if (typeof SystemHealth !== 'undefined') {
                    SystemHealth.load();
                }
                break;
        }
    },

    /**
     * Toggle sidebar
     */
    toggleSidebar() {
        document.getElementById('sidebar')?.classList.toggle('collapsed');
    },

    /**
     * Toggle mobile sidebar
     */
    toggleMobileSidebar() {
        document.getElementById('sidebar')?.classList.toggle('open');
    },

    /**
     * Refresh current page
     */
    refreshCurrentPage() {
        this.loadPageData(AdminState.currentPage);
        AdminToast.success('Refreshed', 'Data has been updated');
    }
};

// ============================================================================
// MODALS
// ============================================================================

const AdminModal = {
    /**
     * Initialize modal
     */
    init() {
        const overlay = document.getElementById('modal-overlay');
        const closeBtn = document.getElementById('modal-close');
        const cancelBtn = document.getElementById('modal-cancel');

        if (overlay) {
            overlay.addEventListener('click', (e) => {
                if (e.target === overlay) this.close();
            });
        }

        if (closeBtn) {
            closeBtn.addEventListener('click', () => this.close());
        }

        if (cancelBtn) {
            cancelBtn.addEventListener('click', () => this.close());
        }

        // Close on escape
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') this.close();
        });
    },

    /**
     * Show modal
     */
    show(options = {}) {
        const { title, content, onConfirm, confirmText = 'Confirm', showFooter = true } = options;

        const overlay = document.getElementById('modal-overlay');
        const titleEl = document.getElementById('modal-title');
        const bodyEl = document.getElementById('modal-body');
        const footerEl = document.getElementById('modal-footer');
        const confirmBtn = document.getElementById('modal-confirm');

        if (titleEl) titleEl.textContent = title || 'Modal';
        if (bodyEl) bodyEl.innerHTML = content || '';
        if (footerEl) footerEl.style.display = showFooter ? 'flex' : 'none';
        if (confirmBtn) {
            confirmBtn.textContent = confirmText;
            confirmBtn.onclick = () => {
                if (onConfirm) onConfirm();
                this.close();
            };
        }

        overlay?.classList.add('active');
    },

    /**
     * Close modal
     */
    close() {
        document.getElementById('modal-overlay')?.classList.remove('active');
    },

    /**
     * Show confirmation modal
     */
    confirm(message, onConfirm) {
        this.show({
            title: 'Confirm Action',
            content: `<p>${message}</p>`,
            onConfirm,
            confirmText: 'Confirm'
        });
    },

    /**
     * Show delete confirmation
     */
    confirmDelete(itemName, onConfirm) {
        this.show({
            title: 'Confirm Delete',
            content: `
                <p>Are you sure you want to delete <strong>${itemName}</strong>?</p>
                <p class="text-danger">This action cannot be undone.</p>
            `,
            onConfirm,
            confirmText: 'Delete'
        });
    }
};

// ============================================================================
// TOAST NOTIFICATIONS
// ============================================================================

const AdminToast = {
    /**
     * Show a toast notification
     */
    show(type, title, message) {
        const container = document.getElementById('toast-container');
        if (!container) return;

        const icons = {
            success: 'ri-checkbox-circle-line',
            error: 'ri-close-circle-line',
            warning: 'ri-alert-line',
            info: 'ri-information-line'
        };

        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.innerHTML = `
            <i class="toast-icon ${icons[type] || icons.info}"></i>
            <div class="toast-content">
                <div class="toast-title">${title}</div>
                ${message ? `<div class="toast-message">${message}</div>` : ''}
            </div>
            <button class="toast-close">
                <i class="ri-close-line"></i>
            </button>
        `;

        const closeBtn = toast.querySelector('.toast-close');
        closeBtn.addEventListener('click', () => this.remove(toast));

        container.appendChild(toast);

        // Auto remove
        setTimeout(() => this.remove(toast), AdminConfig.toastDuration);
    },

    /**
     * Remove a toast
     */
    remove(toast) {
        toast.style.animation = 'slideIn 0.3s ease reverse';
        setTimeout(() => toast.remove(), 300);
    },

    /**
     * Success toast
     */
    success(title, message) {
        this.show('success', title, message);
    },

    /**
     * Error toast
     */
    error(title, message) {
        this.show('error', title, message);
    },

    /**
     * Warning toast
     */
    warning(title, message) {
        this.show('warning', title, message);
    },

    /**
     * Info toast
     */
    info(title, message) {
        this.show('info', title, message);
    }
};

// ============================================================================
// WEBSOCKET
// ============================================================================

const AdminWebSocket = {
    /**
     * Connect to WebSocket
     */
    connect() {
        try {
            AdminState.websocket = new WebSocket(`${AdminConfig.wsBaseUrl}/admin`);

            AdminState.websocket.onopen = () => {
                AdminState.isConnected = true;
                this.updateStatus(true);
                console.log('WebSocket connected');
            };

            AdminState.websocket.onclose = () => {
                AdminState.isConnected = false;
                this.updateStatus(false);
                // Reconnect after 5 seconds
                setTimeout(() => this.connect(), 5000);
            };

            AdminState.websocket.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handleMessage(data);
                } catch (e) {
                    console.error('WebSocket message error:', e);
                }
            };

            AdminState.websocket.onerror = (error) => {
                console.error('WebSocket error:', error);
            };
        } catch (error) {
            console.error('WebSocket connection failed:', error);
        }
    },

    /**
     * Handle WebSocket message
     */
    handleMessage(data) {
        switch (data.type) {
            case 'URGENT_ALERT':
                this.handleUrgentAlert(data);
                break;
            case 'DOG_STATUS_CHANGE':
                this.handleDogStatusChange(data);
                break;
            case 'SYSTEM_ALERT':
                this.handleSystemAlert(data);
                break;
            default:
                console.log('WebSocket message:', data);
        }
    },

    /**
     * Handle urgent alert
     */
    handleUrgentAlert(data) {
        AdminToast.warning('Urgent Alert', data.message);
        // Refresh dashboard if on dashboard page
        if (AdminState.currentPage === 'dashboard') {
            if (typeof AdminDashboard !== 'undefined') {
                AdminDashboard.loadUrgencyOverview();
            }
        }
    },

    /**
     * Handle dog status change
     */
    handleDogStatusChange(data) {
        // Refresh dogs list if on dogs page
        if (AdminState.currentPage === 'dogs') {
            if (typeof DogsManager !== 'undefined') {
                DogsManager.load();
            }
        }
    },

    /**
     * Handle system alert
     */
    handleSystemAlert(data) {
        AdminToast.warning('System Alert', data.message);
        this.updateSystemStatus(data.status);
    },

    /**
     * Update connection status
     */
    updateStatus(connected) {
        const indicator = document.querySelector('.status-indicator');
        const text = document.querySelector('.status-text');

        if (indicator && text) {
            if (connected) {
                indicator.className = 'status-indicator healthy';
                text.textContent = 'System Healthy';
            } else {
                indicator.className = 'status-indicator degraded';
                text.textContent = 'Reconnecting...';
            }
        }
    },

    /**
     * Update system status
     */
    updateSystemStatus(status) {
        const indicator = document.querySelector('.status-indicator');
        const text = document.querySelector('.status-text');

        if (indicator && text) {
            indicator.className = `status-indicator ${status}`;
            text.textContent = status === 'healthy' ? 'System Healthy' :
                              status === 'degraded' ? 'System Degraded' : 'System Unhealthy';
        }
    },

    /**
     * Send message
     */
    send(type, data) {
        if (AdminState.websocket && AdminState.isConnected) {
            AdminState.websocket.send(JSON.stringify({ type, ...data }));
        }
    }
};

// ============================================================================
// TABS
// ============================================================================

const AdminTabs = {
    /**
     * Initialize tabs
     */
    init() {
        document.querySelectorAll('.content-tabs').forEach(tabGroup => {
            tabGroup.querySelectorAll('.tab-btn').forEach(btn => {
                btn.addEventListener('click', () => {
                    const tabId = btn.dataset.tab;
                    this.switchTab(tabGroup, tabId);
                });
            });
        });
    },

    /**
     * Switch tab
     */
    switchTab(tabGroup, tabId) {
        // Update buttons
        tabGroup.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabId);
        });

        // Update content
        const container = tabGroup.parentElement;
        container.querySelectorAll('.tab-content').forEach(content => {
            content.classList.toggle('active', content.id === `tab-${tabId}`);
        });
    }
};

// ============================================================================
// UTILITIES
// ============================================================================

const AdminUtils = {
    /**
     * Format date
     */
    formatDate(dateStr) {
        if (!dateStr) return '-';
        const date = new Date(dateStr);
        return date.toLocaleDateString('en-US', {
            year: 'numeric',
            month: 'short',
            day: 'numeric'
        });
    },

    /**
     * Format datetime
     */
    formatDateTime(dateStr) {
        if (!dateStr) return '-';
        const date = new Date(dateStr);
        return date.toLocaleString('en-US', {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit'
        });
    },

    /**
     * Format relative time
     */
    formatRelativeTime(dateStr) {
        if (!dateStr) return '-';
        const date = new Date(dateStr);
        const now = new Date();
        const diff = now - date;

        const minutes = Math.floor(diff / 60000);
        const hours = Math.floor(diff / 3600000);
        const days = Math.floor(diff / 86400000);

        if (minutes < 1) return 'Just now';
        if (minutes < 60) return `${minutes}m ago`;
        if (hours < 24) return `${hours}h ago`;
        if (days < 7) return `${days}d ago`;

        return this.formatDate(dateStr);
    },

    /**
     * Format number
     */
    formatNumber(num) {
        if (num >= 1000000) return (num / 1000000).toFixed(1) + 'M';
        if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
        return num.toLocaleString();
    },

    /**
     * Debounce function
     */
    debounce(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    },

    /**
     * Escape HTML
     */
    escapeHtml(str) {
        const div = document.createElement('div');
        div.textContent = str;
        return div.innerHTML;
    },

    /**
     * Generate pagination HTML
     */
    generatePagination(current, total, onPageChange) {
        const totalPages = Math.ceil(total / AdminConfig.pageSize);
        if (totalPages <= 1) return '';

        let html = '';

        // Previous button
        html += `<button class="pagination-btn" ${current <= 1 ? 'disabled' : ''} data-page="${current - 1}">
            <i class="ri-arrow-left-s-line"></i>
        </button>`;

        // Page numbers
        const startPage = Math.max(1, current - 2);
        const endPage = Math.min(totalPages, current + 2);

        if (startPage > 1) {
            html += `<button class="pagination-btn" data-page="1">1</button>`;
            if (startPage > 2) html += `<span class="pagination-ellipsis">...</span>`;
        }

        for (let i = startPage; i <= endPage; i++) {
            html += `<button class="pagination-btn ${i === current ? 'active' : ''}" data-page="${i}">${i}</button>`;
        }

        if (endPage < totalPages) {
            if (endPage < totalPages - 1) html += `<span class="pagination-ellipsis">...</span>`;
            html += `<button class="pagination-btn" data-page="${totalPages}">${totalPages}</button>`;
        }

        // Next button
        html += `<button class="pagination-btn" ${current >= totalPages ? 'disabled' : ''} data-page="${current + 1}">
            <i class="ri-arrow-right-s-line"></i>
        </button>`;

        return html;
    },

    /**
     * Bind pagination events
     */
    bindPagination(container, onPageChange) {
        container.querySelectorAll('.pagination-btn:not(:disabled)').forEach(btn => {
            btn.addEventListener('click', () => {
                const page = parseInt(btn.dataset.page);
                if (page && onPageChange) {
                    onPageChange(page);
                }
            });
        });
    }
};

// ============================================================================
// INITIALIZATION
// ============================================================================

document.addEventListener('DOMContentLoaded', async () => {
    // Initialize auth
    const isAuthed = await AdminAuth.init();
    if (!isAuthed) {
        // For development, skip auth check
        // window.location.href = '/login';
        // return;
    }

    // Initialize components
    AdminNav.init();
    AdminModal.init();
    AdminTabs.init();

    // Connect WebSocket
    AdminWebSocket.connect();

    // Load initial page
    AdminNav.handleHashChange();
});

// Export for other modules
window.AdminAPI = AdminAPI;
window.AdminAuth = AdminAuth;
window.AdminNav = AdminNav;
window.AdminModal = AdminModal;
window.AdminToast = AdminToast;
window.AdminUtils = AdminUtils;
window.AdminState = AdminState;
window.AdminConfig = AdminConfig;
