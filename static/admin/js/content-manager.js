/**
 * Content Manager JavaScript
 * WaitingTheLongest.com Admin Dashboard
 * Author: Agent 20
 *
 * Handles content moderation: TikTok posts, blog posts, user submissions
 */

const ContentManager = {
    content: [],
    currentItem: null,
    filters: {
        search: '',
        content_type: '',
        status: 'pending',
        sort: 'submitted_at',
        order: 'desc'
    },
    pagination: {
        page: 1,
        per_page: 20,
        total: 0,
        total_pages: 0
    },
    contentTypes: {
        'tiktok_post': 'TikTok Post',
        'blog_post': 'Blog Post',
        'user_story': 'User Story',
        'shelter_update': 'Shelter Update',
        'event': 'Event',
        'photo': 'Photo',
        'video': 'Video'
    },

    /**
     * Initialize content manager
     */
    init() {
        this.bindEvents();
    },

    /**
     * Load content page
     */
    async load() {
        try {
            await this.loadContent();
            await this.loadContentStats();
        } catch (error) {
            console.error('Content load error:', error);
            AdminToast.error('Error', 'Failed to load content');
        }
    },

    /**
     * Bind event listeners
     */
    bindEvents() {
        // Search input
        const searchInput = document.getElementById('content-search');
        if (searchInput) {
            let debounceTimer;
            searchInput.addEventListener('input', (e) => {
                clearTimeout(debounceTimer);
                debounceTimer = setTimeout(() => {
                    this.filters.search = e.target.value;
                    this.pagination.page = 1;
                    this.loadContent();
                }, 300);
            });
        }

        // Filter selects
        const typeFilter = document.getElementById('content-type-filter');
        if (typeFilter) {
            typeFilter.addEventListener('change', (e) => {
                this.filters.content_type = e.target.value;
                this.pagination.page = 1;
                this.loadContent();
            });
        }

        const statusFilter = document.getElementById('content-status-filter');
        if (statusFilter) {
            statusFilter.addEventListener('change', (e) => {
                this.filters.status = e.target.value;
                this.pagination.page = 1;
                this.loadContent();
            });
        }

        // Bulk actions
        const selectAll = document.getElementById('content-select-all');
        if (selectAll) {
            selectAll.addEventListener('change', (e) => {
                const checkboxes = document.querySelectorAll('.content-checkbox');
                checkboxes.forEach(cb => cb.checked = e.target.checked);
                this.updateBulkActions();
            });
        }

        const bulkApproveBtn = document.getElementById('bulk-approve-btn');
        if (bulkApproveBtn) {
            bulkApproveBtn.addEventListener('click', () => this.bulkApprove());
        }

        const bulkRejectBtn = document.getElementById('bulk-reject-btn');
        if (bulkRejectBtn) {
            bulkRejectBtn.addEventListener('click', () => this.bulkReject());
        }
    },

    /**
     * Load content from API
     */
    async loadContent() {
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

            const response = await AdminAPI.get('/content', params);
            this.content = response.data || [];
            this.pagination = response.pagination || this.pagination;

            this.renderContentGrid();
            this.renderPagination();

        } catch (error) {
            console.error('Load content error:', error);
            this.renderError('Failed to load content');
        }
    },

    /**
     * Load content statistics
     */
    async loadContentStats() {
        try {
            const response = await AdminAPI.get('/dashboard');
            const stats = response.data?.content || {};

            const pendingCount = document.getElementById('pending-content-count');
            if (pendingCount) {
                pendingCount.textContent = stats.pending_moderation || 0;
            }

            const approvedCount = document.getElementById('approved-content-count');
            if (approvedCount) {
                approvedCount.textContent = stats.approved_today || 0;
            }

            const rejectedCount = document.getElementById('rejected-content-count');
            if (rejectedCount) {
                rejectedCount.textContent = stats.rejected_today || 0;
            }

        } catch (error) {
            console.error('Load content stats error:', error);
        }
    },

    /**
     * Render content grid
     */
    renderContentGrid() {
        const container = document.getElementById('content-grid');
        if (!container) return;

        if (this.content.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <i class="ri-file-list-3-line" style="font-size: 64px; color: var(--color-text-light);"></i>
                    <h3>No Content Found</h3>
                    <p>No content items match your current filters</p>
                </div>
            `;
            return;
        }

        container.innerHTML = this.content.map(item => `
            <div class="content-card" data-id="${item.id}">
                <div class="content-card-header">
                    <input type="checkbox" class="content-checkbox" value="${item.id}" onchange="ContentManager.updateBulkActions()">
                    <span class="content-type-badge ${item.content_type}">${this.contentTypes[item.content_type] || item.content_type}</span>
                    <span class="content-status-badge ${item.status}">${item.status}</span>
                </div>
                <div class="content-card-body" onclick="ContentManager.viewContent('${item.id}')">
                    ${this.renderContentPreview(item)}
                    <div class="content-info">
                        <p class="content-preview">${AdminUtils.escapeHtml(this.truncate(item.content_preview || '', 100))}</p>
                        <div class="content-meta">
                            <span><i class="ri-user-line"></i> ${AdminUtils.escapeHtml(item.submitted_by_name || 'Unknown')}</span>
                            <span><i class="ri-time-line"></i> ${AdminUtils.formatRelativeTime(item.submitted_at)}</span>
                        </div>
                    </div>
                </div>
                <div class="content-card-actions">
                    ${item.status === 'pending' ? `
                        <button class="btn btn-success btn-sm" onclick="ContentManager.approveContent('${item.id}')">
                            <i class="ri-check-line"></i> Approve
                        </button>
                        <button class="btn btn-danger btn-sm" onclick="ContentManager.showRejectModal('${item.id}')">
                            <i class="ri-close-line"></i> Reject
                        </button>
                    ` : `
                        <button class="btn btn-secondary btn-sm" onclick="ContentManager.viewContent('${item.id}')">
                            <i class="ri-eye-line"></i> View
                        </button>
                        ${item.status === 'approved' ? `
                            <button class="btn btn-warning btn-sm" onclick="ContentManager.revokeApproval('${item.id}')">
                                <i class="ri-arrow-go-back-line"></i> Revoke
                            </button>
                        ` : ''}
                    `}
                </div>
            </div>
        `).join('');
    },

    /**
     * Render content preview based on type
     */
    renderContentPreview(item) {
        if (item.thumbnail_url) {
            return `<div class="content-thumbnail">
                <img src="${item.thumbnail_url}" alt="Content preview" onerror="this.style.display='none'">
            </div>`;
        }

        if (item.content_type === 'video' || item.content_type === 'tiktok_post') {
            return `<div class="content-thumbnail video-placeholder">
                <i class="ri-video-line"></i>
            </div>`;
        }

        if (item.content_type === 'photo') {
            return `<div class="content-thumbnail image-placeholder">
                <i class="ri-image-line"></i>
            </div>`;
        }

        return `<div class="content-thumbnail text-placeholder">
            <i class="ri-file-text-line"></i>
        </div>`;
    },

    /**
     * Render pagination
     */
    renderPagination() {
        const container = document.getElementById('content-pagination');
        if (!container) return;

        const { page, total_pages, total } = this.pagination;

        if (total_pages <= 1) {
            container.innerHTML = `<span class="pagination-info">Showing ${total} items</span>`;
            return;
        }

        let html = `<span class="pagination-info">Page ${page} of ${total_pages} (${total} items)</span>`;
        html += '<div class="pagination-buttons">';

        html += `<button class="pagination-btn" ${page === 1 ? 'disabled' : ''} onclick="ContentManager.goToPage(${page - 1})">
            <i class="ri-arrow-left-s-line"></i>
        </button>`;

        for (let i = Math.max(1, page - 2); i <= Math.min(total_pages, page + 2); i++) {
            html += `<button class="pagination-btn ${i === page ? 'active' : ''}" onclick="ContentManager.goToPage(${i})">${i}</button>`;
        }

        html += `<button class="pagination-btn" ${page === total_pages ? 'disabled' : ''} onclick="ContentManager.goToPage(${page + 1})">
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
        this.loadContent();
    },

    /**
     * View content details
     */
    async viewContent(id) {
        try {
            const response = await AdminAPI.get(`/content/${id}`);
            const item = response.data;

            this.showContentDetailModal(item);
        } catch (error) {
            console.error('View content error:', error);
            AdminToast.error('Error', 'Failed to load content details');
        }
    },

    /**
     * Show content detail modal
     */
    showContentDetailModal(item) {
        const content = `
            <div class="content-detail">
                <div class="content-detail-header">
                    <span class="content-type-badge ${item.content_type}">${this.contentTypes[item.content_type] || item.content_type}</span>
                    <span class="content-status-badge ${item.status}">${item.status}</span>
                </div>
                <div class="content-detail-body">
                    ${this.renderFullContentPreview(item)}
                    <div class="detail-grid">
                        <div class="detail-item">
                            <label>Content ID</label>
                            <span class="monospace">${item.id}</span>
                        </div>
                        <div class="detail-item">
                            <label>Submitted By</label>
                            <span>${AdminUtils.escapeHtml(item.submitted_by_name || 'Unknown')}</span>
                        </div>
                        <div class="detail-item">
                            <label>Submitted At</label>
                            <span>${item.submitted_at ? new Date(item.submitted_at).toLocaleString() : 'Unknown'}</span>
                        </div>
                        ${item.reviewed_by_name ? `
                            <div class="detail-item">
                                <label>Reviewed By</label>
                                <span>${AdminUtils.escapeHtml(item.reviewed_by_name)}</span>
                            </div>
                            <div class="detail-item">
                                <label>Reviewed At</label>
                                <span>${item.reviewed_at ? new Date(item.reviewed_at).toLocaleString() : 'Unknown'}</span>
                            </div>
                        ` : ''}
                        ${item.rejection_reason ? `
                            <div class="detail-item full-width">
                                <label>Rejection Reason</label>
                                <span class="text-danger">${AdminUtils.escapeHtml(item.rejection_reason)}</span>
                            </div>
                        ` : ''}
                    </div>
                    <div class="detail-section">
                        <label>Content</label>
                        <div class="content-full-text">${AdminUtils.escapeHtml(item.content_preview || 'No content available')}</div>
                    </div>
                    ${item.metadata ? `
                        <div class="detail-section">
                            <label>Metadata</label>
                            <pre class="metadata-json">${JSON.stringify(item.metadata, null, 2)}</pre>
                        </div>
                    ` : ''}
                </div>
            </div>
        `;

        const buttons = [];
        if (item.status === 'pending') {
            buttons.push({ text: 'Approve', class: 'btn-success', action: () => { AdminModal.hide(); this.approveContent(item.id); } });
            buttons.push({ text: 'Reject', class: 'btn-danger', action: () => { AdminModal.hide(); this.showRejectModal(item.id); } });
        } else if (item.status === 'approved') {
            buttons.push({ text: 'Revoke', class: 'btn-warning', action: () => { AdminModal.hide(); this.revokeApproval(item.id); } });
        }
        buttons.push({ text: 'Close', class: 'btn-secondary', action: () => AdminModal.hide() });

        AdminModal.show({
            title: `Content Details`,
            content: content,
            size: 'large',
            buttons: buttons
        });
    },

    /**
     * Render full content preview
     */
    renderFullContentPreview(item) {
        if (item.content_url) {
            if (item.content_type === 'video' || item.content_type === 'tiktok_post') {
                return `<div class="content-media">
                    <video controls>
                        <source src="${item.content_url}" type="video/mp4">
                        Your browser does not support the video tag.
                    </video>
                </div>`;
            }
            if (item.content_type === 'photo') {
                return `<div class="content-media">
                    <img src="${item.content_url}" alt="Content">
                </div>`;
            }
        }

        if (item.thumbnail_url) {
            return `<div class="content-media">
                <img src="${item.thumbnail_url}" alt="Content thumbnail">
            </div>`;
        }

        return '';
    },

    /**
     * Approve content
     */
    async approveContent(id) {
        try {
            await AdminAPI.put(`/content/${id}/approve`, {});
            AdminToast.success('Success', 'Content approved');
            this.loadContent();
            this.loadContentStats();
        } catch (error) {
            console.error('Approve content error:', error);
            AdminToast.error('Error', 'Failed to approve content');
        }
    },

    /**
     * Show reject modal
     */
    showRejectModal(id) {
        const content = `
            <form id="reject-form">
                <div class="form-group">
                    <label for="rejection-reason">Rejection Reason</label>
                    <select id="rejection-reason-select" class="form-control" onchange="document.getElementById('rejection-reason').value = this.value">
                        <option value="">Select a reason or type below</option>
                        <option value="Inappropriate content">Inappropriate content</option>
                        <option value="Spam or promotional">Spam or promotional</option>
                        <option value="Low quality">Low quality</option>
                        <option value="Duplicate content">Duplicate content</option>
                        <option value="Violates guidelines">Violates guidelines</option>
                        <option value="Misleading information">Misleading information</option>
                        <option value="Copyright violation">Copyright violation</option>
                    </select>
                </div>
                <div class="form-group">
                    <textarea id="rejection-reason" class="form-control" rows="3" placeholder="Enter rejection reason..." required></textarea>
                </div>
            </form>
        `;

        AdminModal.show({
            title: 'Reject Content',
            content: content,
            buttons: [
                { text: 'Cancel', class: 'btn-secondary', action: () => AdminModal.hide() },
                { text: 'Reject', class: 'btn-danger', action: () => this.rejectContent(id) }
            ]
        });
    },

    /**
     * Reject content
     */
    async rejectContent(id) {
        try {
            const reason = document.getElementById('rejection-reason').value;
            if (!reason) {
                AdminToast.error('Error', 'Please provide a rejection reason');
                return;
            }

            await AdminAPI.put(`/content/${id}/reject`, { reason });
            AdminToast.success('Success', 'Content rejected');
            AdminModal.hide();
            this.loadContent();
            this.loadContentStats();
        } catch (error) {
            console.error('Reject content error:', error);
            AdminToast.error('Error', 'Failed to reject content');
        }
    },

    /**
     * Revoke approval
     */
    async revokeApproval(id) {
        AdminModal.confirm({
            title: 'Revoke Approval',
            message: 'Are you sure you want to revoke approval for this content? It will be moved back to pending status.',
            confirmText: 'Revoke',
            confirmClass: 'btn-warning',
            onConfirm: async () => {
                try {
                    await AdminAPI.put(`/content/${id}`, { status: 'pending' });
                    AdminToast.success('Success', 'Approval revoked');
                    this.loadContent();
                    this.loadContentStats();
                } catch (error) {
                    console.error('Revoke approval error:', error);
                    AdminToast.error('Error', 'Failed to revoke approval');
                }
            }
        });
    },

    /**
     * Update bulk actions visibility
     */
    updateBulkActions() {
        const checkboxes = document.querySelectorAll('.content-checkbox:checked');
        const bulkActions = document.getElementById('content-bulk-actions');
        const selectedCount = document.getElementById('content-selected-count');

        if (bulkActions) {
            bulkActions.style.display = checkboxes.length > 0 ? 'flex' : 'none';
        }
        if (selectedCount) {
            selectedCount.textContent = checkboxes.length;
        }
    },

    /**
     * Get selected content IDs
     */
    getSelectedIds() {
        const checkboxes = document.querySelectorAll('.content-checkbox:checked');
        return Array.from(checkboxes).map(cb => cb.value);
    },

    /**
     * Bulk approve
     */
    async bulkApprove() {
        const ids = this.getSelectedIds();
        if (ids.length === 0) {
            AdminToast.warning('Warning', 'Please select content to approve');
            return;
        }

        try {
            for (const id of ids) {
                await AdminAPI.put(`/content/${id}/approve`, {});
            }
            AdminToast.success('Success', `Approved ${ids.length} item(s)`);
            this.loadContent();
            this.loadContentStats();
        } catch (error) {
            console.error('Bulk approve error:', error);
            AdminToast.error('Error', 'Failed to approve some items');
        }
    },

    /**
     * Bulk reject
     */
    bulkReject() {
        const ids = this.getSelectedIds();
        if (ids.length === 0) {
            AdminToast.warning('Warning', 'Please select content to reject');
            return;
        }

        const content = `
            <form id="bulk-reject-form">
                <p>Reject ${ids.length} selected item(s)</p>
                <div class="form-group">
                    <label for="bulk-rejection-reason">Rejection Reason</label>
                    <select id="bulk-rejection-reason-select" class="form-control" onchange="document.getElementById('bulk-rejection-reason').value = this.value">
                        <option value="">Select a reason</option>
                        <option value="Inappropriate content">Inappropriate content</option>
                        <option value="Spam or promotional">Spam or promotional</option>
                        <option value="Low quality">Low quality</option>
                        <option value="Violates guidelines">Violates guidelines</option>
                    </select>
                </div>
                <div class="form-group">
                    <textarea id="bulk-rejection-reason" class="form-control" rows="3" placeholder="Enter rejection reason..." required></textarea>
                </div>
            </form>
        `;

        AdminModal.show({
            title: 'Bulk Reject Content',
            content: content,
            buttons: [
                { text: 'Cancel', class: 'btn-secondary', action: () => AdminModal.hide() },
                { text: 'Reject All', class: 'btn-danger', action: async () => {
                    const reason = document.getElementById('bulk-rejection-reason').value;
                    if (!reason) {
                        AdminToast.error('Error', 'Please provide a rejection reason');
                        return;
                    }

                    try {
                        for (const id of ids) {
                            await AdminAPI.put(`/content/${id}/reject`, { reason });
                        }
                        AdminToast.success('Success', `Rejected ${ids.length} item(s)`);
                        AdminModal.hide();
                        this.loadContent();
                        this.loadContentStats();
                    } catch (error) {
                        console.error('Bulk reject error:', error);
                        AdminToast.error('Error', 'Failed to reject some items');
                    }
                }}
            ]
        });
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
     * Render error state
     */
    renderError(message) {
        const container = document.getElementById('content-grid');
        if (container) {
            container.innerHTML = `
                <div class="error-state">
                    <i class="ri-error-warning-line" style="font-size: 64px; color: var(--color-danger);"></i>
                    <h3>Error Loading Content</h3>
                    <p>${AdminUtils.escapeHtml(message)}</p>
                    <button class="btn btn-primary" onclick="ContentManager.loadContent()">Try Again</button>
                </div>
            `;
        }
    }
};

// Export
window.ContentManager = ContentManager;
