/**
 * System Health JavaScript
 * WaitingTheLongest.com Admin Dashboard
 * Author: Agent 20
 *
 * Handles system health monitoring: workers, database, API, logs, configuration
 */

const SystemHealth = {
    health: {},
    workers: [],
    config: {},
    featureFlags: [],
    refreshInterval: null,
    autoRefresh: true,

    /**
     * Initialize system health
     */
    init() {
        this.bindEvents();
    },

    /**
     * Load system health page
     */
    async load() {
        try {
            await Promise.all([
                this.loadHealthStatus(),
                this.loadWorkers(),
                this.loadConfig(),
                this.loadFeatureFlags(),
                this.loadRecentErrors()
            ]);

            if (this.autoRefresh) {
                this.startAutoRefresh();
            }
        } catch (error) {
            console.error('System health load error:', error);
            AdminToast.error('Error', 'Failed to load system health data');
        }
    },

    /**
     * Bind event listeners
     */
    bindEvents() {
        // Auto-refresh toggle
        const autoRefreshToggle = document.getElementById('auto-refresh-toggle');
        if (autoRefreshToggle) {
            autoRefreshToggle.addEventListener('change', (e) => {
                this.autoRefresh = e.target.checked;
                if (this.autoRefresh) {
                    this.startAutoRefresh();
                } else {
                    this.stopAutoRefresh();
                }
            });
        }

        // Manual refresh
        const refreshBtn = document.getElementById('refresh-health-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.load());
        }

        // Config form
        const configForm = document.getElementById('config-form');
        if (configForm) {
            configForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveConfig();
            });
        }

        // Tabs
        const tabBtns = document.querySelectorAll('.health-tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', (e) => {
                this.switchTab(e.target.dataset.tab);
            });
        });
    },

    /**
     * Load health status
     */
    async loadHealthStatus() {
        try {
            const response = await AdminAPI.get('/health');
            this.health = response.data || {};

            this.renderHealthStatus();
        } catch (error) {
            console.error('Load health status error:', error);
            this.health = { status: 'error', error: error.message };
            this.renderHealthStatus();
        }
    },

    /**
     * Render health status
     */
    renderHealthStatus() {
        const container = document.getElementById('health-overview');
        if (!container) return;

        const status = this.health.status || 'unknown';
        const statusClass = status === 'healthy' ? 'success' : status === 'degraded' ? 'warning' : 'danger';
        const statusIcon = status === 'healthy' ? 'ri-checkbox-circle-fill' : status === 'degraded' ? 'ri-error-warning-fill' : 'ri-close-circle-fill';

        container.innerHTML = `
            <div class="health-overview-header ${statusClass}">
                <i class="${statusIcon}"></i>
                <div>
                    <h3>System Status: ${status.charAt(0).toUpperCase() + status.slice(1)}</h3>
                    <p>Last checked: ${new Date().toLocaleTimeString()}</p>
                </div>
            </div>
            <div class="health-metrics-grid">
                <div class="health-metric">
                    <div class="health-metric-icon ${this.health.database?.status === 'healthy' ? 'success' : 'danger'}">
                        <i class="ri-database-2-line"></i>
                    </div>
                    <div class="health-metric-info">
                        <h4>Database</h4>
                        <p>${this.health.database?.status === 'healthy' ? 'Connected' : 'Disconnected'}</p>
                        ${this.health.database?.latency ? `<span class="latency">${this.health.database.latency}ms</span>` : ''}
                    </div>
                </div>
                <div class="health-metric">
                    <div class="health-metric-icon ${this.getWorkerHealthClass()}">
                        <i class="ri-cpu-line"></i>
                    </div>
                    <div class="health-metric-info">
                        <h4>Workers</h4>
                        <p>${this.health.workers?.healthy_count || 0}/${this.health.workers?.total_count || 0} Running</p>
                    </div>
                </div>
                <div class="health-metric">
                    <div class="health-metric-icon ${(this.health.errors?.rate || 0) < 1 ? 'success' : 'warning'}">
                        <i class="ri-error-warning-line"></i>
                    </div>
                    <div class="health-metric-info">
                        <h4>Error Rate</h4>
                        <p>${(this.health.errors?.rate || 0).toFixed(2)}%</p>
                        <span class="sub-metric">${this.health.errors?.count_24h || 0} in 24h</span>
                    </div>
                </div>
                <div class="health-metric">
                    <div class="health-metric-icon success">
                        <i class="ri-server-line"></i>
                    </div>
                    <div class="health-metric-info">
                        <h4>API</h4>
                        <p>Operational</p>
                        ${this.health.api?.avg_response_time ? `<span class="latency">${this.health.api.avg_response_time}ms avg</span>` : ''}
                    </div>
                </div>
                <div class="health-metric">
                    <div class="health-metric-icon ${this.health.cache?.status === 'healthy' ? 'success' : 'warning'}">
                        <i class="ri-stack-line"></i>
                    </div>
                    <div class="health-metric-info">
                        <h4>Cache</h4>
                        <p>${this.health.cache?.hit_rate ? `${this.health.cache.hit_rate}% hit rate` : 'N/A'}</p>
                    </div>
                </div>
                <div class="health-metric">
                    <div class="health-metric-icon success">
                        <i class="ri-hard-drive-2-line"></i>
                    </div>
                    <div class="health-metric-info">
                        <h4>Disk</h4>
                        <p>${this.health.disk?.used_percent || 'N/A'}% used</p>
                    </div>
                </div>
            </div>
        `;
    },

    /**
     * Get worker health class
     */
    getWorkerHealthClass() {
        const healthy = this.health.workers?.healthy_count || 0;
        const total = this.health.workers?.total_count || 0;
        if (healthy === total && total > 0) return 'success';
        if (healthy > 0) return 'warning';
        return 'danger';
    },

    /**
     * Load workers
     */
    async loadWorkers() {
        try {
            const response = await AdminAPI.get('/workers');
            this.workers = response.data || [];

            this.renderWorkers();
        } catch (error) {
            console.error('Load workers error:', error);
        }
    },

    /**
     * Render workers
     */
    renderWorkers() {
        const container = document.getElementById('workers-list');
        if (!container) return;

        if (this.workers.length === 0) {
            container.innerHTML = '<p class="empty-message">No workers configured</p>';
            return;
        }

        container.innerHTML = this.workers.map(worker => `
            <div class="worker-card ${worker.status}">
                <div class="worker-header">
                    <span class="worker-status ${worker.status}">
                        <i class="ri-${worker.status === 'running' ? 'play' : worker.status === 'stopped' ? 'stop' : 'error-warning'}-circle-fill"></i>
                    </span>
                    <h4>${AdminUtils.escapeHtml(worker.name)}</h4>
                </div>
                <div class="worker-info">
                    <p>${AdminUtils.escapeHtml(worker.description || 'No description')}</p>
                    <div class="worker-meta">
                        <span><i class="ri-time-line"></i> Schedule: ${AdminUtils.escapeHtml(worker.schedule || 'Manual')}</span>
                        <span><i class="ri-refresh-line"></i> Last run: ${worker.last_run ? AdminUtils.formatRelativeTime(worker.last_run) : 'Never'}</span>
                        ${worker.next_run ? `<span><i class="ri-calendar-line"></i> Next: ${AdminUtils.formatRelativeTime(worker.next_run)}</span>` : ''}
                    </div>
                    ${worker.last_error ? `<div class="worker-error"><i class="ri-error-warning-line"></i> ${AdminUtils.escapeHtml(worker.last_error)}</div>` : ''}
                </div>
                <div class="worker-actions">
                    ${worker.status === 'running' ?
                        `<button class="btn btn-warning btn-sm" onclick="SystemHealth.stopWorker('${worker.name}')">
                            <i class="ri-stop-line"></i> Stop
                        </button>` :
                        `<button class="btn btn-success btn-sm" onclick="SystemHealth.startWorker('${worker.name}')">
                            <i class="ri-play-line"></i> Start
                        </button>`
                    }
                    <button class="btn btn-secondary btn-sm" onclick="SystemHealth.triggerWorker('${worker.name}')">
                        <i class="ri-refresh-line"></i> Run Now
                    </button>
                </div>
            </div>
        `).join('');
    },

    /**
     * Start worker
     */
    async startWorker(name) {
        try {
            await AdminAPI.post(`/workers/${name}/start`, {});
            AdminToast.success('Success', `Worker "${name}" started`);
            this.loadWorkers();
        } catch (error) {
            console.error('Start worker error:', error);
            AdminToast.error('Error', `Failed to start worker "${name}"`);
        }
    },

    /**
     * Stop worker
     */
    async stopWorker(name) {
        try {
            await AdminAPI.post(`/workers/${name}/stop`, {});
            AdminToast.success('Success', `Worker "${name}" stopped`);
            this.loadWorkers();
        } catch (error) {
            console.error('Stop worker error:', error);
            AdminToast.error('Error', `Failed to stop worker "${name}"`);
        }
    },

    /**
     * Trigger worker manually
     */
    async triggerWorker(name) {
        try {
            await AdminAPI.post(`/workers/${name}/trigger`, {});
            AdminToast.success('Success', `Worker "${name}" triggered`);
            this.loadWorkers();
        } catch (error) {
            console.error('Trigger worker error:', error);
            AdminToast.error('Error', `Failed to trigger worker "${name}"`);
        }
    },

    /**
     * Load configuration
     */
    async loadConfig() {
        try {
            const response = await AdminAPI.get('/config');
            this.config = response.data || {};

            this.renderConfig();
        } catch (error) {
            console.error('Load config error:', error);
        }
    },

    /**
     * Render configuration
     */
    renderConfig() {
        const container = document.getElementById('config-list');
        if (!container) return;

        const configs = Array.isArray(this.config) ? this.config : Object.entries(this.config).map(([key, value]) => ({
            key,
            value: typeof value === 'object' ? value.value : value,
            category: typeof value === 'object' ? value.category : 'general',
            description: typeof value === 'object' ? value.description : '',
            value_type: typeof value === 'object' ? value.value_type : typeof value
        }));

        // Group by category
        const grouped = {};
        for (const config of configs) {
            const category = config.category || 'general';
            if (!grouped[category]) {
                grouped[category] = [];
            }
            grouped[category].push(config);
        }

        let html = '';
        for (const [category, items] of Object.entries(grouped)) {
            html += `
                <div class="config-category">
                    <h4 class="config-category-title">${this.formatCategoryName(category)}</h4>
                    <div class="config-items">
                        ${items.map(config => this.renderConfigItem(config)).join('')}
                    </div>
                </div>
            `;
        }

        container.innerHTML = html || '<p class="empty-message">No configuration loaded</p>';
    },

    /**
     * Render config item
     */
    renderConfigItem(config) {
        const isBoolean = config.value_type === 'bool' || config.value === 'true' || config.value === 'false';
        const isSensitive = config.is_sensitive;

        return `
            <div class="config-item" data-key="${config.key}">
                <div class="config-item-header">
                    <label class="config-key">${AdminUtils.escapeHtml(config.key)}</label>
                    ${config.requires_restart ? '<span class="badge badge-warning">Requires Restart</span>' : ''}
                </div>
                ${config.description ? `<p class="config-description">${AdminUtils.escapeHtml(config.description)}</p>` : ''}
                <div class="config-input-group">
                    ${isBoolean ? `
                        <select class="form-control config-value" data-key="${config.key}">
                            <option value="true" ${config.value === 'true' || config.value === true ? 'selected' : ''}>True</option>
                            <option value="false" ${config.value === 'false' || config.value === false ? 'selected' : ''}>False</option>
                        </select>
                    ` : `
                        <input type="${isSensitive ? 'password' : 'text'}"
                               class="form-control config-value"
                               data-key="${config.key}"
                               value="${AdminUtils.escapeHtml(String(config.value || ''))}"
                               ${isSensitive ? 'autocomplete="off"' : ''}>
                    `}
                    <button class="btn btn-primary btn-sm" onclick="SystemHealth.updateConfigItem('${config.key}')">
                        <i class="ri-save-line"></i>
                    </button>
                </div>
            </div>
        `;
    },

    /**
     * Format category name
     */
    formatCategoryName(category) {
        return category.split('_').map(word => word.charAt(0).toUpperCase() + word.slice(1)).join(' ');
    },

    /**
     * Update config item
     */
    async updateConfigItem(key) {
        try {
            const input = document.querySelector(`.config-value[data-key="${key}"]`);
            if (!input) return;

            const value = input.value;
            await AdminAPI.put('/config', { key, value });

            AdminToast.success('Success', `Configuration "${key}" updated`);
        } catch (error) {
            console.error('Update config error:', error);
            AdminToast.error('Error', `Failed to update "${key}"`);
        }
    },

    /**
     * Load feature flags
     */
    async loadFeatureFlags() {
        try {
            const response = await AdminAPI.get('/feature-flags');
            this.featureFlags = response.data || [];

            this.renderFeatureFlags();
        } catch (error) {
            console.error('Load feature flags error:', error);
        }
    },

    /**
     * Render feature flags
     */
    renderFeatureFlags() {
        const container = document.getElementById('feature-flags-list');
        if (!container) return;

        if (this.featureFlags.length === 0) {
            container.innerHTML = '<p class="empty-message">No feature flags configured</p>';
            return;
        }

        container.innerHTML = this.featureFlags.map(flag => `
            <div class="feature-flag-card ${flag.enabled ? 'enabled' : 'disabled'}">
                <div class="feature-flag-header">
                    <div class="feature-flag-toggle">
                        <label class="toggle-switch">
                            <input type="checkbox"
                                   ${flag.enabled ? 'checked' : ''}
                                   onchange="SystemHealth.toggleFeatureFlag('${flag.key}', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                    <div class="feature-flag-info">
                        <h4>${AdminUtils.escapeHtml(flag.name || flag.key)}</h4>
                        <p>${AdminUtils.escapeHtml(flag.description || 'No description')}</p>
                    </div>
                </div>
                <div class="feature-flag-details">
                    <span class="flag-key"><i class="ri-key-2-line"></i> ${flag.key}</span>
                    <span class="flag-rollout"><i class="ri-percent-line"></i> ${flag.rollout_percentage}% rollout</span>
                </div>
                ${flag.enabled && flag.rollout_percentage < 100 ? `
                    <div class="rollout-slider">
                        <input type="range" min="0" max="100" value="${flag.rollout_percentage}"
                               onchange="SystemHealth.updateRollout('${flag.key}', this.value)">
                        <span class="rollout-value">${flag.rollout_percentage}%</span>
                    </div>
                ` : ''}
            </div>
        `).join('');
    },

    /**
     * Toggle feature flag
     */
    async toggleFeatureFlag(key, enabled) {
        try {
            await AdminAPI.put(`/feature-flags/${key}`, { enabled });
            AdminToast.success('Success', `Feature "${key}" ${enabled ? 'enabled' : 'disabled'}`);
            this.loadFeatureFlags();
        } catch (error) {
            console.error('Toggle feature flag error:', error);
            AdminToast.error('Error', `Failed to toggle feature "${key}"`);
        }
    },

    /**
     * Update rollout percentage
     */
    async updateRollout(key, percentage) {
        try {
            await AdminAPI.put(`/feature-flags/${key}`, { rollout_percentage: parseInt(percentage) });
            AdminToast.success('Success', `Rollout updated to ${percentage}%`);
        } catch (error) {
            console.error('Update rollout error:', error);
            AdminToast.error('Error', 'Failed to update rollout');
        }
    },

    /**
     * Load recent errors
     */
    async loadRecentErrors() {
        try {
            const response = await AdminAPI.get('/errors', { per_page: 20 });
            const errors = response.data || [];

            this.renderErrors(errors);
        } catch (error) {
            console.error('Load errors error:', error);
        }
    },

    /**
     * Render errors
     */
    renderErrors(errors) {
        const container = document.getElementById('errors-list');
        if (!container) return;

        if (errors.length === 0) {
            container.innerHTML = '<p class="empty-message">No recent errors</p>';
            return;
        }

        container.innerHTML = `
            <table class="errors-table">
                <thead>
                    <tr>
                        <th>Time</th>
                        <th>Type</th>
                        <th>Message</th>
                        <th>Source</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody>
                    ${errors.map(error => `
                        <tr class="error-row ${error.severity || 'error'}">
                            <td>${AdminUtils.formatRelativeTime(error.created_at)}</td>
                            <td><span class="error-type">${AdminUtils.escapeHtml(error.type || 'Error')}</span></td>
                            <td class="error-message">${AdminUtils.escapeHtml(this.truncate(error.message, 60))}</td>
                            <td>${AdminUtils.escapeHtml(error.source || 'Unknown')}</td>
                            <td>
                                <button class="action-btn" onclick="SystemHealth.viewError('${error.id}')" title="View Details">
                                    <i class="ri-eye-line"></i>
                                </button>
                            </td>
                        </tr>
                    `).join('')}
                </tbody>
            </table>
        `;
    },

    /**
     * View error details
     */
    async viewError(id) {
        try {
            const response = await AdminAPI.get(`/errors/${id}`);
            const error = response.data;

            const content = `
                <div class="error-detail">
                    <div class="detail-grid">
                        <div class="detail-item">
                            <label>Error ID</label>
                            <span class="monospace">${error.id}</span>
                        </div>
                        <div class="detail-item">
                            <label>Type</label>
                            <span>${AdminUtils.escapeHtml(error.type || 'Error')}</span>
                        </div>
                        <div class="detail-item">
                            <label>Severity</label>
                            <span class="severity-badge ${error.severity}">${error.severity || 'error'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Source</label>
                            <span>${AdminUtils.escapeHtml(error.source || 'Unknown')}</span>
                        </div>
                        <div class="detail-item">
                            <label>Occurred At</label>
                            <span>${error.created_at ? new Date(error.created_at).toLocaleString() : 'Unknown'}</span>
                        </div>
                        <div class="detail-item">
                            <label>User</label>
                            <span>${AdminUtils.escapeHtml(error.user_email || 'System')}</span>
                        </div>
                    </div>
                    <div class="detail-section">
                        <label>Message</label>
                        <p class="error-message-full">${AdminUtils.escapeHtml(error.message)}</p>
                    </div>
                    ${error.stack_trace ? `
                        <div class="detail-section">
                            <label>Stack Trace</label>
                            <pre class="stack-trace">${AdminUtils.escapeHtml(error.stack_trace)}</pre>
                        </div>
                    ` : ''}
                    ${error.context ? `
                        <div class="detail-section">
                            <label>Context</label>
                            <pre class="error-context">${JSON.stringify(error.context, null, 2)}</pre>
                        </div>
                    ` : ''}
                </div>
            `;

            AdminModal.show({
                title: 'Error Details',
                content: content,
                size: 'large',
                buttons: [
                    { text: 'Close', class: 'btn-secondary', action: () => AdminModal.hide() }
                ]
            });
        } catch (error) {
            console.error('View error details error:', error);
            AdminToast.error('Error', 'Failed to load error details');
        }
    },

    /**
     * Switch tab
     */
    switchTab(tabId) {
        // Update tab buttons
        document.querySelectorAll('.health-tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabId);
        });

        // Update tab content
        document.querySelectorAll('.health-tab-content').forEach(content => {
            content.classList.toggle('active', content.id === `${tabId}-tab`);
        });
    },

    /**
     * Start auto refresh
     */
    startAutoRefresh() {
        this.stopAutoRefresh();
        this.refreshInterval = setInterval(() => {
            this.loadHealthStatus();
            this.loadWorkers();
        }, 30000); // 30 seconds
    },

    /**
     * Stop auto refresh
     */
    stopAutoRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    },

    /**
     * Truncate text
     */
    truncate(text, length) {
        if (!text) return '';
        if (text.length <= length) return text;
        return text.substring(0, length) + '...';
    },

    /**
     * Cleanup on page leave
     */
    cleanup() {
        this.stopAutoRefresh();
    }
};

// Export
window.SystemHealth = SystemHealth;
