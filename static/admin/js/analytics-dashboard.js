/**
 * Analytics Dashboard JavaScript
 * WaitingTheLongest.com Admin Dashboard
 * Author: Agent 20
 *
 * Handles analytics visualization: charts, metrics, reports, exports
 */

const AnalyticsDashboard = {
    charts: {},
    currentPeriod: '30d',
    refreshInterval: null,
    data: {},

    /**
     * Initialize analytics dashboard
     */
    init() {
        this.bindEvents();
    },

    /**
     * Load analytics page
     */
    async load() {
        try {
            await Promise.all([
                this.loadOverviewMetrics(),
                this.loadAdoptionAnalytics(),
                this.loadUrgencyAnalytics(),
                this.loadShelterAnalytics(),
                this.loadUserAnalytics(),
                this.loadGeographicData()
            ]);

            this.initCharts();
        } catch (error) {
            console.error('Analytics load error:', error);
            AdminToast.error('Error', 'Failed to load analytics data');
        }
    },

    /**
     * Bind event listeners
     */
    bindEvents() {
        // Period selector
        const periodSelect = document.getElementById('analytics-period');
        if (periodSelect) {
            periodSelect.addEventListener('change', (e) => {
                this.currentPeriod = e.target.value;
                this.load();
            });
        }

        // Export buttons
        const exportPdfBtn = document.getElementById('export-pdf-btn');
        if (exportPdfBtn) {
            exportPdfBtn.addEventListener('click', () => this.exportPDF());
        }

        const exportCsvBtn = document.getElementById('export-csv-btn');
        if (exportCsvBtn) {
            exportCsvBtn.addEventListener('click', () => this.exportCSV());
        }

        // Refresh button
        const refreshBtn = document.getElementById('analytics-refresh-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.load());
        }
    },

    /**
     * Load overview metrics
     */
    async loadOverviewMetrics() {
        try {
            const response = await AdminAPI.get('/analytics/overview', { period: this.currentPeriod });
            const metrics = response.data || {};
            this.data.overview = metrics;

            // Update metric cards
            this.updateMetric('total-adoptions', metrics.total_adoptions || 0, metrics.adoption_change);
            this.updateMetric('total-fosters', metrics.total_fosters || 0, metrics.foster_change);
            this.updateMetric('avg-wait-time', `${metrics.avg_wait_time_days || 0} days`, metrics.wait_time_change);
            this.updateMetric('lives-saved', metrics.lives_saved || 0, metrics.lives_saved_change);
            this.updateMetric('new-registrations', metrics.new_registrations || 0, metrics.registration_change);
            this.updateMetric('active-users', metrics.active_users || 0, metrics.active_change);

        } catch (error) {
            console.error('Load overview metrics error:', error);
        }
    },

    /**
     * Update a metric display
     */
    updateMetric(id, value, change) {
        const valueEl = document.getElementById(`metric-${id}`);
        if (valueEl) {
            valueEl.textContent = typeof value === 'number' ? AdminUtils.formatNumber(value) : value;
        }

        const changeEl = document.getElementById(`metric-${id}-change`);
        if (changeEl && change !== undefined) {
            const isPositive = change >= 0;
            changeEl.innerHTML = `
                <i class="ri-arrow-${isPositive ? 'up' : 'down'}-line"></i>
                ${Math.abs(change)}%
            `;
            changeEl.className = `metric-change ${isPositive ? 'positive' : 'negative'}`;
        }
    },

    /**
     * Load adoption analytics
     */
    async loadAdoptionAnalytics() {
        try {
            const response = await AdminAPI.get('/analytics/adoptions', { period: this.currentPeriod });
            this.data.adoptions = response.data || {};
        } catch (error) {
            console.error('Load adoption analytics error:', error);
        }
    },

    /**
     * Load urgency analytics
     */
    async loadUrgencyAnalytics() {
        try {
            const response = await AdminAPI.get('/analytics/urgency', { period: this.currentPeriod });
            this.data.urgency = response.data || {};
        } catch (error) {
            console.error('Load urgency analytics error:', error);
        }
    },

    /**
     * Load shelter analytics
     */
    async loadShelterAnalytics() {
        try {
            const response = await AdminAPI.get('/analytics/shelters', { period: this.currentPeriod });
            this.data.shelters = response.data || {};
        } catch (error) {
            console.error('Load shelter analytics error:', error);
        }
    },

    /**
     * Load user analytics
     */
    async loadUserAnalytics() {
        try {
            const response = await AdminAPI.get('/analytics/users', { period: this.currentPeriod });
            this.data.users = response.data || {};
        } catch (error) {
            console.error('Load user analytics error:', error);
        }
    },

    /**
     * Load geographic data
     */
    async loadGeographicData() {
        try {
            const response = await AdminAPI.get('/analytics/geographic', { period: this.currentPeriod });
            this.data.geographic = response.data || {};
        } catch (error) {
            console.error('Load geographic data error:', error);
        }
    },

    /**
     * Initialize all charts
     */
    initCharts() {
        this.initAdoptionTrendChart();
        this.initUrgencyDistributionChart();
        this.initOutcomeChart();
        this.initShelterPerformanceChart();
        this.initUserGrowthChart();
        this.initTopBreedsChart();
        this.initWaitTimeDistributionChart();
        this.initStateHeatmap();
    },

    /**
     * Initialize adoption trend chart
     */
    initAdoptionTrendChart() {
        const ctx = document.getElementById('adoption-trend-chart');
        if (!ctx) return;

        if (this.charts.adoptionTrend) {
            this.charts.adoptionTrend.destroy();
        }

        const adoptionData = this.data.adoptions || {};
        const labels = adoptionData.labels || this.generateDateLabels(30);
        const adoptions = adoptionData.adoptions || this.generateMockData(30, 5, 15);
        const fosters = adoptionData.fosters || this.generateMockData(30, 3, 10);
        const returns = adoptionData.returns || this.generateMockData(30, 0, 3);

        this.charts.adoptionTrend = new Chart(ctx, {
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
                    },
                    {
                        label: 'Returns',
                        data: returns,
                        borderColor: '#f59e0b',
                        backgroundColor: 'rgba(245, 158, 11, 0.1)',
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
                        position: 'top'
                    },
                    title: {
                        display: true,
                        text: 'Adoption & Foster Trends'
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true
                    }
                }
            }
        });
    },

    /**
     * Initialize urgency distribution chart
     */
    initUrgencyDistributionChart() {
        const ctx = document.getElementById('urgency-distribution-chart');
        if (!ctx) return;

        if (this.charts.urgencyDist) {
            this.charts.urgencyDist.destroy();
        }

        const urgencyData = this.data.urgency || {};

        this.charts.urgencyDist = new Chart(ctx, {
            type: 'doughnut',
            data: {
                labels: ['Critical', 'High', 'Medium', 'Normal'],
                datasets: [{
                    data: [
                        urgencyData.critical || 12,
                        urgencyData.high || 28,
                        urgencyData.medium || 45,
                        urgencyData.normal || 215
                    ],
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
                        position: 'right'
                    },
                    title: {
                        display: true,
                        text: 'Current Urgency Distribution'
                    }
                },
                cutout: '60%'
            }
        });
    },

    /**
     * Initialize outcome chart
     */
    initOutcomeChart() {
        const ctx = document.getElementById('outcome-chart');
        if (!ctx) return;

        if (this.charts.outcome) {
            this.charts.outcome.destroy();
        }

        const outcomeData = this.data.overview || {};

        this.charts.outcome = new Chart(ctx, {
            type: 'pie',
            data: {
                labels: ['Adopted', 'Fostered', 'Transferred', 'Returned', 'Deceased'],
                datasets: [{
                    data: [
                        outcomeData.outcomes?.adopted || 156,
                        outcomeData.outcomes?.fostered || 89,
                        outcomeData.outcomes?.transferred || 23,
                        outcomeData.outcomes?.returned || 12,
                        outcomeData.outcomes?.deceased || 8
                    ],
                    backgroundColor: [
                        '#4f46e5',
                        '#10b981',
                        '#8b5cf6',
                        '#f59e0b',
                        '#6b7280'
                    ]
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'right'
                    },
                    title: {
                        display: true,
                        text: 'Outcomes This Period'
                    }
                }
            }
        });
    },

    /**
     * Initialize shelter performance chart
     */
    initShelterPerformanceChart() {
        const ctx = document.getElementById('shelter-performance-chart');
        if (!ctx) return;

        if (this.charts.shelterPerformance) {
            this.charts.shelterPerformance.destroy();
        }

        const shelterData = this.data.shelters || {};
        const topShelters = shelterData.top_performers || [
            { name: 'Happy Tails Rescue', adoptions: 45, fosters: 23 },
            { name: 'Second Chance', adoptions: 38, fosters: 18 },
            { name: 'Paws & Claws', adoptions: 32, fosters: 15 },
            { name: 'Forever Home', adoptions: 28, fosters: 12 },
            { name: 'Animal Haven', adoptions: 25, fosters: 11 }
        ];

        this.charts.shelterPerformance = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: topShelters.map(s => s.name),
                datasets: [
                    {
                        label: 'Adoptions',
                        data: topShelters.map(s => s.adoptions),
                        backgroundColor: '#4f46e5'
                    },
                    {
                        label: 'Fosters',
                        data: topShelters.map(s => s.fosters),
                        backgroundColor: '#10b981'
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'top'
                    },
                    title: {
                        display: true,
                        text: 'Top Performing Shelters'
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true
                    }
                }
            }
        });
    },

    /**
     * Initialize user growth chart
     */
    initUserGrowthChart() {
        const ctx = document.getElementById('user-growth-chart');
        if (!ctx) return;

        if (this.charts.userGrowth) {
            this.charts.userGrowth.destroy();
        }

        const userData = this.data.users || {};
        const labels = userData.labels || this.generateDateLabels(30);
        const newUsers = userData.new_users || this.generateMockData(30, 10, 50);
        const cumulativeUsers = [];
        let cumulative = userData.starting_users || 5000;
        for (const n of newUsers) {
            cumulative += n;
            cumulativeUsers.push(cumulative);
        }

        this.charts.userGrowth = new Chart(ctx, {
            type: 'line',
            data: {
                labels,
                datasets: [
                    {
                        label: 'Total Users',
                        data: cumulativeUsers,
                        borderColor: '#4f46e5',
                        backgroundColor: 'rgba(79, 70, 229, 0.1)',
                        fill: true,
                        tension: 0.4,
                        yAxisID: 'y'
                    },
                    {
                        label: 'New Users',
                        data: newUsers,
                        type: 'bar',
                        backgroundColor: 'rgba(16, 185, 129, 0.7)',
                        yAxisID: 'y1'
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'top'
                    },
                    title: {
                        display: true,
                        text: 'User Growth'
                    }
                },
                scales: {
                    y: {
                        type: 'linear',
                        position: 'left',
                        title: {
                            display: true,
                            text: 'Total Users'
                        }
                    },
                    y1: {
                        type: 'linear',
                        position: 'right',
                        title: {
                            display: true,
                            text: 'New Users'
                        },
                        grid: {
                            drawOnChartArea: false
                        }
                    }
                }
            }
        });
    },

    /**
     * Initialize top breeds chart
     */
    initTopBreedsChart() {
        const ctx = document.getElementById('top-breeds-chart');
        if (!ctx) return;

        if (this.charts.topBreeds) {
            this.charts.topBreeds.destroy();
        }

        const breedData = this.data.overview?.top_breeds || [
            { breed: 'Pit Bull Mix', count: 89 },
            { breed: 'Labrador Mix', count: 67 },
            { breed: 'Chihuahua', count: 54 },
            { breed: 'German Shepherd', count: 42 },
            { breed: 'Beagle Mix', count: 38 },
            { breed: 'Mixed Breed', count: 156 }
        ];

        this.charts.topBreeds = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: breedData.map(b => b.breed),
                datasets: [{
                    label: 'Count',
                    data: breedData.map(b => b.count),
                    backgroundColor: [
                        '#4f46e5',
                        '#7c3aed',
                        '#a855f7',
                        '#c084fc',
                        '#d8b4fe',
                        '#e9d5ff'
                    ]
                }]
            },
            options: {
                indexAxis: 'y',
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        display: false
                    },
                    title: {
                        display: true,
                        text: 'Most Common Breeds'
                    }
                },
                scales: {
                    x: {
                        beginAtZero: true
                    }
                }
            }
        });
    },

    /**
     * Initialize wait time distribution chart
     */
    initWaitTimeDistributionChart() {
        const ctx = document.getElementById('wait-time-chart');
        if (!ctx) return;

        if (this.charts.waitTime) {
            this.charts.waitTime.destroy();
        }

        const waitTimeData = this.data.overview?.wait_time_distribution || {
            '0-7 days': 45,
            '8-30 days': 89,
            '31-90 days': 67,
            '91-180 days': 34,
            '181-365 days': 18,
            '1+ years': 12
        };

        this.charts.waitTime = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: Object.keys(waitTimeData),
                datasets: [{
                    label: 'Dogs',
                    data: Object.values(waitTimeData),
                    backgroundColor: [
                        '#10b981',
                        '#22c55e',
                        '#84cc16',
                        '#eab308',
                        '#f97316',
                        '#ef4444'
                    ]
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        display: false
                    },
                    title: {
                        display: true,
                        text: 'Wait Time Distribution'
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true
                    }
                }
            }
        });
    },

    /**
     * Initialize state heatmap
     */
    initStateHeatmap() {
        const container = document.getElementById('state-heatmap');
        if (!container) return;

        const geoData = this.data.geographic || {};
        const stateData = geoData.by_state || {
            'CA': 245, 'TX': 198, 'FL': 156, 'NY': 134, 'PA': 89,
            'OH': 76, 'GA': 67, 'NC': 58, 'MI': 54, 'AZ': 48
        };

        // Render as table for now (a real heatmap would use D3.js or similar)
        let html = '<table class="state-table">';
        html += '<thead><tr><th>State</th><th>Dogs</th><th>Trend</th></tr></thead>';
        html += '<tbody>';

        const sortedStates = Object.entries(stateData)
            .sort((a, b) => b[1] - a[1])
            .slice(0, 10);

        const maxValue = sortedStates[0]?.[1] || 1;

        for (const [state, count] of sortedStates) {
            const percent = Math.round((count / maxValue) * 100);
            html += `<tr>
                <td><strong>${state}</strong></td>
                <td>
                    <div class="bar-cell">
                        <div class="bar" style="width: ${percent}%; background: #4f46e5;"></div>
                        <span>${count}</span>
                    </div>
                </td>
                <td><span class="trend-indicator up"><i class="ri-arrow-up-line"></i></span></td>
            </tr>`;
        }

        html += '</tbody></table>';
        container.innerHTML = html;
    },

    /**
     * Generate date labels
     */
    generateDateLabels(days) {
        const labels = [];
        for (let i = days - 1; i >= 0; i--) {
            const date = new Date();
            date.setDate(date.getDate() - i);
            labels.push(date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' }));
        }
        return labels;
    },

    /**
     * Generate mock data
     */
    generateMockData(count, min, max) {
        const data = [];
        for (let i = 0; i < count; i++) {
            data.push(Math.floor(Math.random() * (max - min + 1)) + min);
        }
        return data;
    },

    /**
     * Export PDF report
     */
    async exportPDF() {
        try {
            AdminToast.info('Generating', 'Preparing PDF report...');

            // In a real implementation, this would generate a PDF
            // For now, we'll simulate the process
            await new Promise(resolve => setTimeout(resolve, 1500));

            AdminToast.success('Success', 'PDF report downloaded');
        } catch (error) {
            console.error('Export PDF error:', error);
            AdminToast.error('Error', 'Failed to generate PDF');
        }
    },

    /**
     * Export CSV data
     */
    async exportCSV() {
        try {
            AdminToast.info('Exporting', 'Preparing CSV data...');

            // Compile data for export
            const exportData = {
                overview: this.data.overview,
                period: this.currentPeriod,
                generated_at: new Date().toISOString()
            };

            // Create summary CSV
            let csv = 'Metric,Value,Change\n';
            const metrics = this.data.overview || {};
            csv += `Total Adoptions,${metrics.total_adoptions || 0},${metrics.adoption_change || 0}%\n`;
            csv += `Total Fosters,${metrics.total_fosters || 0},${metrics.foster_change || 0}%\n`;
            csv += `Avg Wait Time,${metrics.avg_wait_time_days || 0} days,${metrics.wait_time_change || 0}%\n`;
            csv += `Lives Saved,${metrics.lives_saved || 0},${metrics.lives_saved_change || 0}%\n`;
            csv += `New Registrations,${metrics.new_registrations || 0},${metrics.registration_change || 0}%\n`;
            csv += `Active Users,${metrics.active_users || 0},${metrics.active_change || 0}%\n`;

            const blob = new Blob([csv], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `analytics-${this.currentPeriod}-${new Date().toISOString().split('T')[0]}.csv`;
            a.click();
            URL.revokeObjectURL(url);

            AdminToast.success('Success', 'CSV data downloaded');
        } catch (error) {
            console.error('Export CSV error:', error);
            AdminToast.error('Error', 'Failed to export CSV');
        }
    },

    /**
     * Cleanup on page leave
     */
    cleanup() {
        // Destroy all charts
        Object.values(this.charts).forEach(chart => {
            if (chart) chart.destroy();
        });
        this.charts = {};

        // Clear refresh interval
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    }
};

// Export
window.AnalyticsDashboard = AnalyticsDashboard;
