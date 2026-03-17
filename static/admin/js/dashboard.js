/**
 * Dashboard JavaScript
 * WaitingTheLongest.com Admin Dashboard
 * Author: Agent 20
 *
 * Handles dashboard statistics, charts, and real-time updates
 */

const AdminDashboard = {
    charts: {},
    refreshInterval: null,

    /**
     * Load dashboard data
     */
    async load() {
        try {
            await Promise.all([
                this.loadStats(),
                this.loadUrgencyOverview(),
                this.loadRecentActivity(),
                this.loadUrgentAlerts(),
                this.loadSystemHealth()
            ]);

            this.initCharts();
            this.startAutoRefresh();
        } catch (error) {
            console.error('Dashboard load error:', error);
            AdminToast.error('Error', 'Failed to load dashboard data');
        }
    },

    /**
     * Load main statistics
     */
    async loadStats() {
        try {
            const response = await AdminAPI.get('/dashboard');
            const stats = response.data;

            // Update stat cards
            this.updateStat('stat-total-dogs', stats.counts?.total_dogs || 0);
            this.updateStat('stat-urgent-dogs', stats.urgency?.total_urgent || 0);
            this.updateStat('stat-critical-count', `${stats.urgency?.critical || 0} critical`);
            this.updateStat('stat-total-shelters', stats.counts?.total_shelters || 0);
            this.updateStat('stat-verified-shelters', `${stats.counts?.verified_shelters || 0} verified`);
            this.updateStat('stat-total-users', stats.counts?.total_users || 0);
            this.updateStat('stat-active-users', `${stats.active_users?.active_users || 0} active today`);
            this.updateStat('stat-foster-homes', stats.counts?.foster_homes || 0);
            this.updateStat('stat-active-placements', `${stats.counts?.active_placements || 0} active placements`);

            // Update badges
            const urgentBadge = document.getElementById('urgent-dogs-badge');
            if (urgentBadge) {
                urgentBadge.textContent = stats.urgency?.total_urgent || 0;
                urgentBadge.style.display = (stats.urgency?.total_urgent || 0) > 0 ? 'inline' : 'none';
            }

        } catch (error) {
            console.error('Stats load error:', error);
        }
    },

    /**
     * Update a stat element
     */
    updateStat(id, value) {
        const el = document.getElementById(id);
        if (el) {
            if (typeof value === 'number') {
                el.textContent = AdminUtils.formatNumber(value);
            } else {
                el.textContent = value;
            }
        }
    },

    /**
     * Load urgency overview for charts
     */
    async loadUrgencyOverview() {
        try {
            const response = await AdminAPI.get('/dashboard');
            const urgency = response.data?.urgency || {};

            // Update chart data
            if (this.charts.urgency) {
                this.charts.urgency.data.datasets[0].data = [
                    urgency.critical || 0,
                    urgency.high || 0,
                    urgency.medium || 0,
                    urgency.normal || 0
                ];
                this.charts.urgency.update();
            }

            // Update alerts count
            const alertsCount = document.getElementById('alerts-count');
            if (alertsCount) {
                alertsCount.textContent = (urgency.critical || 0) + (urgency.high || 0);
            }

        } catch (error) {
            console.error('Urgency overview error:', error);
        }
    },

    /**
     * Load recent activity
     */
    async loadRecentActivity() {
        try {
            const response = await AdminAPI.get('/audit-log', { per_page: 10 });
            const entries = response.data || [];

            const list = document.getElementById('recent-activity');
            if (!list) return;

            if (entries.length === 0) {
                list.innerHTML = '<li class="activity-item">No recent activity</li>';
                return;
            }

            list.innerHTML = entries.map(entry => `
                <li class="activity-item">
                    <div class="activity-icon" style="background: ${this.getActivityColor(entry.action)}">
                        <i class="${this.getActivityIcon(entry.action)}"></i>
                    </div>
                    <div class="activity-content">
                        <div class="activity-text">
                            <strong>${entry.user_email || 'Unknown'}</strong>
                            ${this.formatAction(entry.action)} ${entry.entity_type}
                        </div>
                        <div class="activity-time">${AdminUtils.formatRelativeTime(entry.created_at)}</div>
                    </div>
                </li>
            `).join('');

        } catch (error) {
            console.error('Recent activity error:', error);
            const list = document.getElementById('recent-activity');
            if (list) {
                list.innerHTML = '<li class="activity-item">Failed to load activity</li>';
            }
        }
    },

    /**
     * Load urgent alerts
     */
    async loadUrgentAlerts() {
        try {
            const response = await AdminAPI.get('/api/urgency/alerts/critical'.replace('/api/admin', ''));
            const alerts = response.data || [];

            const list = document.getElementById('urgent-alerts');
            if (!list) return;

            if (alerts.length === 0) {
                list.innerHTML = '<li class="alert-item">No urgent alerts</li>';
                return;
            }

            list.innerHTML = alerts.slice(0, 5).map(alert => `
                <li class="alert-item ${alert.type || 'critical'}">
                    <i class="ri-alarm-warning-fill" style="color: var(--color-danger);"></i>
                    <div class="alert-content">
                        <div class="alert-text">${AdminUtils.escapeHtml(alert.message || alert.dog_name || 'Alert')}</div>
                        <div class="alert-time">${AdminUtils.formatRelativeTime(alert.created_at)}</div>
                    </div>
                    <button class="action-btn" onclick="AdminDashboard.viewAlert('${alert.id}')">
                        <i class="ri-eye-line"></i>
                    </button>
                </li>
            `).join('');

        } catch (error) {
            console.error('Urgent alerts error:', error);
            const list = document.getElementById('urgent-alerts');
            if (list) {
                list.innerHTML = '<li class="alert-item">Failed to load alerts</li>';
            }
        }
    },

    /**
     * Load system health
     */
    async loadSystemHealth() {
        try {
            const response = await AdminAPI.get('/health');
            const health = response.data || {};

            // Update overall status
            const overallHealth = document.getElementById('overall-health');
            if (overallHealth) {
                overallHealth.textContent = health.status === 'healthy' ? 'Healthy' :
                                            health.status === 'degraded' ? 'Degraded' : 'Unhealthy';
                overallHealth.className = `health-status ${health.status || 'healthy'}`;
            }

            // Update system status in header
            const statusIndicator = document.querySelector('.status-indicator');
            const statusText = document.querySelector('.status-text');
            if (statusIndicator && statusText) {
                statusIndicator.className = `status-indicator ${health.status || 'healthy'}`;
                statusText.textContent = health.status === 'healthy' ? 'System Healthy' :
                                         health.status === 'degraded' ? 'System Degraded' : 'System Unhealthy';
            }

            // Update health indicators
            const indicators = document.getElementById('health-indicators');
            if (indicators) {
                indicators.innerHTML = `
                    <div class="health-item">
                        <div class="health-icon">
                            <i class="ri-database-2-line"></i>
                        </div>
                        <div class="health-info">
                            <span class="health-name">Database</span>
                            <span class="health-value ${health.database?.status || 'healthy'}">
                                ${health.database?.status === 'healthy' ? 'Connected' : 'Disconnected'}
                            </span>
                        </div>
                    </div>
                    <div class="health-item">
                        <div class="health-icon">
                            <i class="ri-time-line"></i>
                        </div>
                        <div class="health-info">
                            <span class="health-name">Workers</span>
                            <span class="health-value healthy">
                                ${health.workers?.healthy_count || 0}/${health.workers?.total_count || 0} Running
                            </span>
                        </div>
                    </div>
                    <div class="health-item">
                        <div class="health-icon">
                            <i class="ri-error-warning-line"></i>
                        </div>
                        <div class="health-info">
                            <span class="health-name">Error Rate</span>
                            <span class="health-value ${health.errors?.rate > 1 ? 'warning' : 'healthy'}">
                                ${(health.errors?.rate || 0).toFixed(2)}%
                            </span>
                        </div>
                    </div>
                    <div class="health-item">
                        <div class="health-icon">
                            <i class="ri-server-line"></i>
                        </div>
                        <div class="health-info">
                            <span class="health-name">API Status</span>
                            <span class="health-value healthy">Operational</span>
                        </div>
                    </div>
                `;
            }

        } catch (error) {
            console.error('System health error:', error);
        }
    },

    /**
     * Initialize charts
     */
    initCharts() {
        this.initUrgencyChart();
        this.initAdoptionChart();
    },

    /**
     * Initialize urgency chart
     */
    initUrgencyChart() {
        const ctx = document.getElementById('urgency-chart');
        if (!ctx) return;

        // Destroy existing chart
        if (this.charts.urgency) {
            this.charts.urgency.destroy();
        }

        this.charts.urgency = new Chart(ctx, {
            type: 'doughnut',
            data: {
                labels: ['Critical', 'High', 'Medium', 'Normal'],
                datasets: [{
                    data: [0, 0, 0, 0],
                    backgroundColor: [
                        '#dc2626',
                        '#ea580c',
                        '#d97706',
                        '#16a34a'
                    ],
                    borderWidth: 0
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'right',
                        labels: {
                            usePointStyle: true,
                            padding: 20
                        }
                    }
                },
                cutout: '60%'
            }
        });

        // Period selector
        const periodSelect = document.getElementById('urgency-period');
        if (periodSelect) {
            periodSelect.addEventListener('change', () => {
                this.loadUrgencyOverview();
            });
        }
    },

    /**
     * Initialize adoption chart
     */
    initAdoptionChart() {
        const ctx = document.getElementById('adoption-chart');
        if (!ctx) return;

        // Destroy existing chart
        if (this.charts.adoption) {
            this.charts.adoption.destroy();
        }

        // Generate mock data for now
        const labels = [];
        const adoptions = [];
        const fosters = [];

        for (let i = 29; i >= 0; i--) {
            const date = new Date();
            date.setDate(date.getDate() - i);
            labels.push(date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' }));
            adoptions.push(Math.floor(Math.random() * 10) + 5);
            fosters.push(Math.floor(Math.random() * 8) + 3);
        }

        this.charts.adoption = new Chart(ctx, {
            type: 'line',
            data: {
                labels,
                datasets: [
                    {
                        label: 'Adoptions',
                        data: adoptions,
                        borderColor: '#4f46e5',
                        backgroundColor: 'rgba(79, 70, 229, 0.1)',
                        fill: true,
                        tension: 0.4
                    },
                    {
                        label: 'Fosters',
                        data: fosters,
                        borderColor: '#10b981',
                        backgroundColor: 'rgba(16, 185, 129, 0.1)',
                        fill: true,
                        tension: 0.4
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'top',
                        labels: {
                            usePointStyle: true
                        }
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)'
                        }
                    },
                    x: {
                        grid: {
                            display: false
                        }
                    }
                }
            }
        });

        // Period selector
        const periodSelect = document.getElementById('adoption-period');
        if (periodSelect) {
            periodSelect.addEventListener('change', () => {
                this.loadAdoptionData(periodSelect.value);
            });
        }
    },

    /**
     * Load adoption data for specific period
     */
    async loadAdoptionData(period) {
        try {
            const response = await AdminAPI.get('/analytics', { period });
            const data = response.data || {};

            // Update chart with real data
            // For now, using mock data
            AdminToast.info('Loading', 'Fetching adoption data...');
        } catch (error) {
            console.error('Adoption data error:', error);
        }
    },

    /**
     * Get activity icon
     */
    getActivityIcon(action) {
        const icons = {
            'UPDATE_DOG': 'ri-edit-line',
            'DELETE_DOG': 'ri-delete-bin-line',
            'CREATE_DOG': 'ri-add-line',
            'UPDATE_USER': 'ri-user-settings-line',
            'CHANGE_USER_ROLE': 'ri-shield-user-line',
            'UPDATE_SHELTER': 'ri-home-gear-line',
            'VERIFY_SHELTER': 'ri-checkbox-circle-line',
            'UPDATE_CONFIG': 'ri-settings-line',
            'VIEW_DASHBOARD': 'ri-dashboard-line'
        };
        return icons[action] || 'ri-file-list-line';
    },

    /**
     * Get activity color
     */
    getActivityColor(action) {
        if (action.includes('DELETE')) return '#fee2e2';
        if (action.includes('CREATE') || action.includes('ADD')) return '#d1fae5';
        if (action.includes('UPDATE') || action.includes('CHANGE')) return '#dbeafe';
        return '#f3f4f6';
    },

    /**
     * Format action text
     */
    formatAction(action) {
        return action.toLowerCase().replace(/_/g, ' ');
    },

    /**
     * View alert details
     */
    async viewAlert(alertId) {
        try {
            // Navigate to the dog or show modal
            AdminToast.info('Alert', `Viewing alert ${alertId}`);
        } catch (error) {
            console.error('View alert error:', error);
        }
    },

    /**
     * Start auto refresh
     */
    startAutoRefresh() {
        // Clear existing interval
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
        }

        // Refresh every minute
        this.refreshInterval = setInterval(() => {
            if (AdminState.currentPage === 'dashboard') {
                this.loadStats();
                this.loadUrgencyOverview();
                this.loadSystemHealth();
            }
        }, AdminConfig.refreshInterval);
    },

    /**
     * Stop auto refresh
     */
    stopAutoRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    }
};

// Export
window.AdminDashboard = AdminDashboard;
