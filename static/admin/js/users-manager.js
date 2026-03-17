/**
 * Users Manager JavaScript
 * WaitingTheLongest.com Admin Dashboard
 * Author: Agent 20
 *
 * Handles user management: list, roles, permissions, activity, status
 */

const UsersManager = {
    users: [],
    currentUser: null,
    roles: ['user', 'volunteer', 'shelter_admin', 'moderator', 'admin', 'super_admin'],
    filters: {
        search: '',
        role: '',
        status: '',
        verified: '',
        sort: 'created_at',
        order: 'desc'
    },
    pagination: {
        page: 1,
        per_page: 20,
        total: 0,
        total_pages: 0
    },

    /**
     * Initialize users manager
     */
    init() {
        this.bindEvents();
    },

    /**
     * Load users page
     */
    async load() {
        try {
            await this.loadUsers();
            await this.loadUserStats();
        } catch (error) {
            console.error('Users load error:', error);
            AdminToast.error('Error', 'Failed to load users');
        }
    },

    /**
     * Bind event listeners
     */
    bindEvents() {
        // Search input
        const searchInput = document.getElementById('users-search');
        if (searchInput) {
            let debounceTimer;
            searchInput.addEventListener('input', (e) => {
                clearTimeout(debounceTimer);
                debounceTimer = setTimeout(() => {
                    this.filters.search = e.target.value;
                    this.pagination.page = 1;
                    this.loadUsers();
                }, 300);
            });
        }

        // Filter selects
        const filterIds = ['users-role-filter', 'users-status-filter', 'users-verified-filter'];
        filterIds.forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.addEventListener('change', (e) => {
                    const filterKey = id.replace('users-', '').replace('-filter', '');
                    this.filters[filterKey] = e.target.value;
                    this.pagination.page = 1;
                    this.loadUsers();
                });
            }
        });

        // Sort select
        const sortSelect = document.getElementById('users-sort');
        if (sortSelect) {
            sortSelect.addEventListener('change', (e) => {
                const [sort, order] = e.target.value.split(':');
                this.filters.sort = sort;
                this.filters.order = order || 'asc';
                this.loadUsers();
            });
        }

        // Add user button
        const addBtn = document.getElementById('add-user-btn');
        if (addBtn) {
            addBtn.addEventListener('click', () => this.showAddUserModal());
        }

        // Export button
        const exportBtn = document.getElementById('export-users-btn');
        if (exportBtn) {
            exportBtn.addEventListener('click', () => this.exportUsers());
        }

        // Modal form submission
        const userForm = document.getElementById('user-form');
        if (userForm) {
            userForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveUser();
            });
        }
    },

    /**
     * Load users from API
     */
    async loadUsers() {
        try {
            const params = {
                page: this.pagination.page,
                per_page: this.pagination.per_page,
                ...this.filters
            };

            // Remove empty filters
            Object.keys(params).forEach(key => {
                if (params[key] === '' || params[key] === null) {
                    delete params[key];
                }
            });

            const response = await AdminAPI.get('/users', params);
            this.users = response.data || [];
            this.pagination = response.pagination || this.pagination;

            this.renderUsersTable();
            this.renderPagination();

        } catch (error) {
            console.error('Load users error:', error);
            this.renderError('Failed to load users');
        }
    },

    /**
     * Load user statistics
     */
    async loadUserStats() {
        try {
            const response = await AdminAPI.get('/dashboard');
            const stats = response.data || {};

            const totalUsers = document.getElementById('total-users-count');
            if (totalUsers) {
                totalUsers.textContent = stats.counts?.total_users || 0;
            }

            const activeUsers = document.getElementById('active-users-count');
            if (activeUsers) {
                activeUsers.textContent = stats.active_users?.active_users || 0;
            }

            const newUsers = document.getElementById('new-users-count');
            if (newUsers) {
                newUsers.textContent = stats.counts?.new_users_today || 0;
            }

            const adminUsers = document.getElementById('admin-users-count');
            if (adminUsers) {
                adminUsers.textContent = stats.counts?.admin_count || 0;
            }

        } catch (error) {
            console.error('Load user stats error:', error);
        }
    },

    /**
     * Render users table
     */
    renderUsersTable() {
        const tbody = document.getElementById('users-table-body');
        if (!tbody) return;

        if (this.users.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="8" class="text-center py-8">
                        <div class="empty-state">
                            <i class="ri-user-line" style="font-size: 48px; color: var(--color-text-light);"></i>
                            <p>No users found matching your criteria</p>
                        </div>
                    </td>
                </tr>
            `;
            return;
        }

        tbody.innerHTML = this.users.map(user => `
            <tr data-id="${user.id}">
                <td>
                    <div class="user-cell">
                        <div class="user-avatar" style="background: ${this.getAvatarColor(user.email)}">
                            ${this.getInitials(user.name || user.email)}
                        </div>
                        <div class="user-info">
                            <strong>${AdminUtils.escapeHtml(user.name || 'Unknown')}</strong>
                            <span class="user-email">${AdminUtils.escapeHtml(user.email)}</span>
                        </div>
                    </div>
                </td>
                <td>
                    <span class="role-badge role-${user.role || 'user'}">
                        ${this.formatRole(user.role)}
                    </span>
                </td>
                <td>
                    ${user.email_verified ?
                        '<span class="badge badge-success"><i class="ri-mail-check-line"></i> Verified</span>' :
                        '<span class="badge badge-warning"><i class="ri-mail-close-line"></i> Unverified</span>'
                    }
                </td>
                <td>
                    ${user.is_active ?
                        '<span class="status-indicator active"></span> Active' :
                        '<span class="status-indicator inactive"></span> Inactive'
                    }
                </td>
                <td>
                    ${user.last_login ? AdminUtils.formatRelativeTime(user.last_login) : 'Never'}
                </td>
                <td>
                    ${AdminUtils.formatRelativeTime(user.created_at)}
                </td>
                <td>
                    ${user.shelter_name ? AdminUtils.escapeHtml(user.shelter_name) : '-'}
                </td>
                <td>
                    <div class="action-buttons">
                        <button class="action-btn" onclick="UsersManager.viewUser('${user.id}')" title="View">
                            <i class="ri-eye-line"></i>
                        </button>
                        <button class="action-btn" onclick="UsersManager.editUser('${user.id}')" title="Edit">
                            <i class="ri-edit-line"></i>
                        </button>
                        <button class="action-btn" onclick="UsersManager.showRoleModal('${user.id}')" title="Change Role">
                            <i class="ri-shield-user-line"></i>
                        </button>
                        ${user.is_active ?
                            `<button class="action-btn warning" onclick="UsersManager.deactivateUser('${user.id}')" title="Deactivate">
                                <i class="ri-user-unfollow-line"></i>
                            </button>` :
                            `<button class="action-btn success" onclick="UsersManager.activateUser('${user.id}')" title="Activate">
                                <i class="ri-user-follow-line"></i>
                            </button>`
                        }
                        <button class="action-btn danger" onclick="UsersManager.confirmDelete('${user.id}')" title="Delete">
                            <i class="ri-delete-bin-line"></i>
                        </button>
                    </div>
                </td>
            </tr>
        `).join('');
    },

    /**
     * Render pagination
     */
    renderPagination() {
        const container = document.getElementById('users-pagination');
        if (!container) return;

        const { page, total_pages, total } = this.pagination;

        if (total_pages <= 1) {
            container.innerHTML = `<span class="pagination-info">Showing ${total} users</span>`;
            return;
        }

        let html = `<span class="pagination-info">Page ${page} of ${total_pages} (${total} users)</span>`;
        html += '<div class="pagination-buttons">';

        html += `<button class="pagination-btn" ${page === 1 ? 'disabled' : ''} onclick="UsersManager.goToPage(${page - 1})">
            <i class="ri-arrow-left-s-line"></i>
        </button>`;

        const maxButtons = 5;
        let startPage = Math.max(1, page - Math.floor(maxButtons / 2));
        let endPage = Math.min(total_pages, startPage + maxButtons - 1);

        if (endPage - startPage < maxButtons - 1) {
            startPage = Math.max(1, endPage - maxButtons + 1);
        }

        if (startPage > 1) {
            html += `<button class="pagination-btn" onclick="UsersManager.goToPage(1)">1</button>`;
            if (startPage > 2) {
                html += '<span class="pagination-ellipsis">...</span>';
            }
        }

        for (let i = startPage; i <= endPage; i++) {
            html += `<button class="pagination-btn ${i === page ? 'active' : ''}" onclick="UsersManager.goToPage(${i})">${i}</button>`;
        }

        if (endPage < total_pages) {
            if (endPage < total_pages - 1) {
                html += '<span class="pagination-ellipsis">...</span>';
            }
            html += `<button class="pagination-btn" onclick="UsersManager.goToPage(${total_pages})">${total_pages}</button>`;
        }

        html += `<button class="pagination-btn" ${page === total_pages ? 'disabled' : ''} onclick="UsersManager.goToPage(${page + 1})">
            <i class="ri-arrow-right-s-line"></i>
        </button>`;

        html += '</div>';
        container.innerHTML = html;
    },

    /**
     * Go to specific page
     */
    goToPage(page) {
        if (page < 1 || page > this.pagination.total_pages) return;
        this.pagination.page = page;
        this.loadUsers();
    },

    /**
     * Get avatar background color based on email
     */
    getAvatarColor(email) {
        const colors = [
            '#4f46e5', '#7c3aed', '#db2777', '#dc2626',
            '#ea580c', '#d97706', '#65a30d', '#059669',
            '#0891b2', '#0284c7', '#2563eb', '#4f46e5'
        ];
        let hash = 0;
        for (let i = 0; i < email.length; i++) {
            hash = email.charCodeAt(i) + ((hash << 5) - hash);
        }
        return colors[Math.abs(hash) % colors.length];
    },

    /**
     * Get initials from name or email
     */
    getInitials(name) {
        if (!name) return '?';
        const parts = name.split(/[\s@]+/);
        if (parts.length >= 2) {
            return (parts[0][0] + parts[1][0]).toUpperCase();
        }
        return name.substring(0, 2).toUpperCase();
    },

    /**
     * Format role display
     */
    formatRole(role) {
        const roleLabels = {
            'user': 'User',
            'volunteer': 'Volunteer',
            'shelter_admin': 'Shelter Admin',
            'moderator': 'Moderator',
            'admin': 'Admin',
            'super_admin': 'Super Admin'
        };
        return roleLabels[role] || role || 'User';
    },

    /**
     * View user details
     */
    async viewUser(id) {
        try {
            const response = await AdminAPI.get(`/users/${id}`);
            const user = response.data;

            this.showUserDetailModal(user);
        } catch (error) {
            console.error('View user error:', error);
            AdminToast.error('Error', 'Failed to load user details');
        }
    },

    /**
     * Show user detail modal
     */
    showUserDetailModal(user) {
        const content = `
            <div class="user-detail">
                <div class="user-detail-header">
                    <div class="user-avatar large" style="background: ${this.getAvatarColor(user.email)}">
                        ${this.getInitials(user.name || user.email)}
                    </div>
                    <div class="user-detail-info">
                        <h2>${AdminUtils.escapeHtml(user.name || 'Unknown User')}</h2>
                        <p class="user-email">${AdminUtils.escapeHtml(user.email)}</p>
                        <span class="role-badge role-${user.role || 'user'}">
                            ${this.formatRole(user.role)}
                        </span>
                        ${user.is_active ?
                            '<span class="badge badge-success">Active</span>' :
                            '<span class="badge badge-danger">Inactive</span>'
                        }
                    </div>
                </div>
                <div class="user-detail-body">
                    <div class="detail-grid">
                        <div class="detail-item">
                            <label>User ID</label>
                            <span class="monospace">${user.id}</span>
                        </div>
                        <div class="detail-item">
                            <label>Email Verified</label>
                            <span>${user.email_verified ? 'Yes' : 'No'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Phone</label>
                            <span>${AdminUtils.escapeHtml(user.phone || 'Not provided')}</span>
                        </div>
                        <div class="detail-item">
                            <label>Associated Shelter</label>
                            <span>${user.shelter_name ? AdminUtils.escapeHtml(user.shelter_name) : 'None'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Registered</label>
                            <span>${user.created_at ? new Date(user.created_at).toLocaleString() : 'Unknown'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Last Login</label>
                            <span>${user.last_login ? new Date(user.last_login).toLocaleString() : 'Never'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Last Updated</label>
                            <span>${user.updated_at ? AdminUtils.formatRelativeTime(user.updated_at) : 'Never'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Login Count</label>
                            <span>${user.login_count || 0}</span>
                        </div>
                    </div>
                    ${user.bio ? `
                        <div class="detail-section">
                            <label>Bio</label>
                            <p>${AdminUtils.escapeHtml(user.bio)}</p>
                        </div>
                    ` : ''}
                    <div class="detail-section">
                        <label>Recent Activity</label>
                        <div id="user-recent-activity" class="activity-list">
                            <p class="text-muted">Loading activity...</p>
                        </div>
                    </div>
                </div>
            </div>
        `;

        AdminModal.show({
            title: `User Details: ${user.name || user.email}`,
            content: content,
            size: 'large',
            buttons: [
                { text: 'Edit', class: 'btn-primary', action: () => { AdminModal.hide(); this.editUser(user.id); } },
                { text: 'View Activity', class: 'btn-secondary', action: () => this.viewUserActivity(user.id) },
                { text: 'Close', class: 'btn-secondary', action: () => AdminModal.hide() }
            ]
        });

        // Load recent activity
        this.loadUserRecentActivity(user.id);
    },

    /**
     * Load user recent activity
     */
    async loadUserRecentActivity(userId) {
        try {
            const response = await AdminAPI.get('/audit-log', { user_id: userId, per_page: 5 });
            const entries = response.data || [];

            const container = document.getElementById('user-recent-activity');
            if (!container) return;

            if (entries.length === 0) {
                container.innerHTML = '<p class="text-muted">No recent activity</p>';
                return;
            }

            container.innerHTML = entries.map(entry => `
                <div class="activity-item">
                    <span class="activity-action">${entry.action}</span>
                    <span class="activity-entity">${entry.entity_type}</span>
                    <span class="activity-time">${AdminUtils.formatRelativeTime(entry.created_at)}</span>
                </div>
            `).join('');

        } catch (error) {
            console.error('Load user activity error:', error);
        }
    },

    /**
     * View full user activity
     */
    viewUserActivity(userId) {
        // Navigate to audit log with user filter
        AdminNav.navigate('audit-log');
        setTimeout(() => {
            if (typeof AuditLogManager !== 'undefined') {
                AuditLogManager.filters.user_id = userId;
                AuditLogManager.loadAuditLog();
            }
        }, 100);
    },

    /**
     * Show add user modal
     */
    showAddUserModal() {
        this.currentUser = null;
        this.resetForm();
        document.getElementById('user-modal-title').textContent = 'Add New User';
        document.getElementById('user-password-group').style.display = 'block';
        document.getElementById('user-password').required = true;
        AdminModal.show({ id: 'user-modal' });
    },

    /**
     * Edit user
     */
    async editUser(id) {
        try {
            const response = await AdminAPI.get(`/users/${id}`);
            this.currentUser = response.data;

            this.populateForm(this.currentUser);
            document.getElementById('user-modal-title').textContent = 'Edit User';
            document.getElementById('user-password-group').style.display = 'block';
            document.getElementById('user-password').required = false;
            document.getElementById('user-password').placeholder = 'Leave blank to keep current';
            AdminModal.show({ id: 'user-modal' });
        } catch (error) {
            console.error('Edit user error:', error);
            AdminToast.error('Error', 'Failed to load user for editing');
        }
    },

    /**
     * Reset form
     */
    resetForm() {
        const form = document.getElementById('user-form');
        if (form) {
            form.reset();
        }
        document.getElementById('user-password').placeholder = 'Enter password';
    },

    /**
     * Populate form with user data
     */
    populateForm(user) {
        const fields = ['name', 'email', 'phone', 'role', 'bio'];

        fields.forEach(field => {
            const input = document.getElementById(`user-${field}`);
            if (input) {
                input.value = user[field] || '';
            }
        });

        // Handle checkboxes
        const isActiveCheckbox = document.getElementById('user-is-active');
        if (isActiveCheckbox) {
            isActiveCheckbox.checked = user.is_active !== false;
        }

        const emailVerifiedCheckbox = document.getElementById('user-email-verified');
        if (emailVerifiedCheckbox) {
            emailVerifiedCheckbox.checked = user.email_verified || false;
        }

        // Handle shelter association
        const shelterSelect = document.getElementById('user-shelter-id');
        if (shelterSelect && user.shelter_id) {
            shelterSelect.value = user.shelter_id;
        }
    },

    /**
     * Save user (create or update)
     */
    async saveUser() {
        try {
            const formData = this.getFormData();

            if (!this.validateForm(formData)) {
                return;
            }

            let response;
            if (this.currentUser) {
                // Remove password if empty (don't update)
                if (!formData.password) {
                    delete formData.password;
                }
                response = await AdminAPI.put(`/users/${this.currentUser.id}`, formData);
                AdminToast.success('Success', 'User updated successfully');
            } else {
                response = await AdminAPI.post('/users', formData);
                AdminToast.success('Success', 'User created successfully');
            }

            AdminModal.hide('user-modal');
            this.loadUsers();
        } catch (error) {
            console.error('Save user error:', error);
            AdminToast.error('Error', error.message || 'Failed to save user');
        }
    },

    /**
     * Get form data
     */
    getFormData() {
        const form = document.getElementById('user-form');
        const data = {};

        const textFields = ['name', 'email', 'phone', 'role', 'bio', 'password', 'shelter_id'];
        textFields.forEach(field => {
            const input = form.querySelector(`#user-${field.replace('_', '-')}`);
            if (input && input.value.trim()) {
                data[field] = input.value.trim();
            }
        });

        // Handle checkboxes
        const isActiveCheckbox = form.querySelector('#user-is-active');
        if (isActiveCheckbox) {
            data.is_active = isActiveCheckbox.checked;
        }

        const emailVerifiedCheckbox = form.querySelector('#user-email-verified');
        if (emailVerifiedCheckbox) {
            data.email_verified = emailVerifiedCheckbox.checked;
        }

        return data;
    },

    /**
     * Validate form
     */
    validateForm(data) {
        if (!data.name) {
            AdminToast.error('Validation Error', 'Name is required');
            return false;
        }
        if (!data.email) {
            AdminToast.error('Validation Error', 'Email is required');
            return false;
        }
        if (data.email && !AdminUtils.isValidEmail(data.email)) {
            AdminToast.error('Validation Error', 'Please enter a valid email address');
            return false;
        }
        if (!this.currentUser && !data.password) {
            AdminToast.error('Validation Error', 'Password is required for new users');
            return false;
        }
        if (data.password && data.password.length < 8) {
            AdminToast.error('Validation Error', 'Password must be at least 8 characters');
            return false;
        }
        return true;
    },

    /**
     * Show role change modal
     */
    showRoleModal(id) {
        const user = this.users.find(u => u.id === id);
        if (!user) return;

        const content = `
            <form id="role-form">
                <p>Change role for <strong>${AdminUtils.escapeHtml(user.name || user.email)}</strong></p>
                <div class="form-group">
                    <label for="new-role">New Role</label>
                    <select id="new-role" class="form-control" required>
                        ${this.roles.map(role => `
                            <option value="${role}" ${user.role === role ? 'selected' : ''}>
                                ${this.formatRole(role)}
                            </option>
                        `).join('')}
                    </select>
                </div>
                <div class="form-group">
                    <label for="role-reason">Reason (optional)</label>
                    <textarea id="role-reason" class="form-control" rows="2" placeholder="Reason for role change..."></textarea>
                </div>
            </form>
        `;

        AdminModal.show({
            title: 'Change User Role',
            content: content,
            buttons: [
                { text: 'Cancel', class: 'btn-secondary', action: () => AdminModal.hide() },
                { text: 'Change Role', class: 'btn-primary', action: () => this.changeRole(id) }
            ]
        });
    },

    /**
     * Change user role
     */
    async changeRole(id) {
        try {
            const role = document.getElementById('new-role').value;
            const reason = document.getElementById('role-reason').value;

            await AdminAPI.put(`/users/${id}/role`, { role, reason });

            AdminToast.success('Success', 'User role updated successfully');
            AdminModal.hide();
            this.loadUsers();
        } catch (error) {
            console.error('Change role error:', error);
            AdminToast.error('Error', 'Failed to change user role');
        }
    },

    /**
     * Activate user
     */
    async activateUser(id) {
        try {
            await AdminAPI.put(`/users/${id}`, { is_active: true });
            AdminToast.success('Success', 'User activated successfully');
            this.loadUsers();
        } catch (error) {
            console.error('Activate user error:', error);
            AdminToast.error('Error', 'Failed to activate user');
        }
    },

    /**
     * Deactivate user
     */
    async deactivateUser(id) {
        const user = this.users.find(u => u.id === id);
        if (!user) return;

        AdminModal.confirm({
            title: 'Deactivate User',
            message: `Are you sure you want to deactivate "${user.name || user.email}"? They will no longer be able to log in.`,
            confirmText: 'Deactivate',
            confirmClass: 'btn-warning',
            onConfirm: async () => {
                try {
                    await AdminAPI.put(`/users/${id}`, { is_active: false });
                    AdminToast.success('Success', 'User deactivated');
                    this.loadUsers();
                } catch (error) {
                    console.error('Deactivate user error:', error);
                    AdminToast.error('Error', 'Failed to deactivate user');
                }
            }
        });
    },

    /**
     * Confirm delete user
     */
    confirmDelete(id) {
        const user = this.users.find(u => u.id === id);
        if (!user) return;

        AdminModal.confirm({
            title: 'Delete User',
            message: `Are you sure you want to delete "${user.name || user.email}"? This action cannot be undone.`,
            confirmText: 'Delete',
            confirmClass: 'btn-danger',
            onConfirm: () => this.deleteUser(id)
        });
    },

    /**
     * Delete user
     */
    async deleteUser(id) {
        try {
            await AdminAPI.delete(`/users/${id}`);
            AdminToast.success('Success', 'User deleted successfully');
            this.loadUsers();
        } catch (error) {
            console.error('Delete user error:', error);
            AdminToast.error('Error', 'Failed to delete user');
        }
    },

    /**
     * Send password reset
     */
    async sendPasswordReset(id) {
        try {
            await AdminAPI.post(`/users/${id}/password-reset`, {});
            AdminToast.success('Success', 'Password reset email sent');
        } catch (error) {
            console.error('Password reset error:', error);
            AdminToast.error('Error', 'Failed to send password reset');
        }
    },

    /**
     * Force logout user
     */
    async forceLogout(id) {
        try {
            await AdminAPI.post(`/users/${id}/force-logout`, {});
            AdminToast.success('Success', 'User sessions terminated');
        } catch (error) {
            console.error('Force logout error:', error);
            AdminToast.error('Error', 'Failed to force logout');
        }
    },

    /**
     * Export users
     */
    async exportUsers() {
        try {
            AdminToast.info('Exporting', 'Preparing export...');

            const params = { ...this.filters, per_page: 10000 };
            const response = await AdminAPI.get('/users', params);
            const users = response.data || [];

            if (users.length === 0) {
                AdminToast.warning('Warning', 'No users to export');
                return;
            }

            const headers = ['ID', 'Name', 'Email', 'Role', 'Verified', 'Active', 'Last Login', 'Created'];
            const rows = users.map(user => [
                user.id,
                user.name || '',
                user.email,
                user.role || 'user',
                user.email_verified ? 'Yes' : 'No',
                user.is_active !== false ? 'Yes' : 'No',
                user.last_login || 'Never',
                user.created_at || ''
            ]);

            const csv = [headers, ...rows].map(row => row.map(cell => `"${String(cell).replace(/"/g, '""')}"`).join(',')).join('\n');

            const blob = new Blob([csv], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `users-export-${new Date().toISOString().split('T')[0]}.csv`;
            a.click();
            URL.revokeObjectURL(url);

            AdminToast.success('Success', `Exported ${users.length} users`);
        } catch (error) {
            console.error('Export error:', error);
            AdminToast.error('Error', 'Failed to export users');
        }
    },

    /**
     * Render error state
     */
    renderError(message) {
        const tbody = document.getElementById('users-table-body');
        if (tbody) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="8" class="text-center py-8">
                        <div class="error-state">
                            <i class="ri-error-warning-line" style="font-size: 48px; color: var(--color-danger);"></i>
                            <p>${AdminUtils.escapeHtml(message)}</p>
                            <button class="btn btn-primary" onclick="UsersManager.loadUsers()">Try Again</button>
                        </div>
                    </td>
                </tr>
            `;
        }
    }
};

// Export
window.UsersManager = UsersManager;
