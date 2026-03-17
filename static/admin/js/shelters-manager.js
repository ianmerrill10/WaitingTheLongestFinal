/**
 * Shelters Manager JavaScript
 * WaitingTheLongest.com Admin Dashboard
 * Author: Agent 20
 *
 * Handles shelter management: list, verify, edit, contact info, statistics
 */

const SheltersManager = {
    shelters: [],
    currentShelter: null,
    filters: {
        search: '',
        verified: '',
        state: '',
        has_urgent: '',
        sort: 'name',
        order: 'asc'
    },
    pagination: {
        page: 1,
        per_page: 20,
        total: 0,
        total_pages: 0
    },

    /**
     * Initialize shelters manager
     */
    init() {
        this.bindEvents();
        this.loadStateOptions();
    },

    /**
     * Load shelters page
     */
    async load() {
        try {
            await this.loadShelters();
            await this.loadShelterStats();
        } catch (error) {
            console.error('Shelters load error:', error);
            AdminToast.error('Error', 'Failed to load shelters');
        }
    },

    /**
     * Bind event listeners
     */
    bindEvents() {
        // Search input
        const searchInput = document.getElementById('shelters-search');
        if (searchInput) {
            let debounceTimer;
            searchInput.addEventListener('input', (e) => {
                clearTimeout(debounceTimer);
                debounceTimer = setTimeout(() => {
                    this.filters.search = e.target.value;
                    this.pagination.page = 1;
                    this.loadShelters();
                }, 300);
            });
        }

        // Filter selects
        const filterIds = ['shelters-verified-filter', 'shelters-state-filter', 'shelters-urgent-filter'];
        filterIds.forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.addEventListener('change', (e) => {
                    const filterKey = id.replace('shelters-', '').replace('-filter', '').replace('-', '_');
                    this.filters[filterKey] = e.target.value;
                    this.pagination.page = 1;
                    this.loadShelters();
                });
            }
        });

        // Sort select
        const sortSelect = document.getElementById('shelters-sort');
        if (sortSelect) {
            sortSelect.addEventListener('change', (e) => {
                const [sort, order] = e.target.value.split(':');
                this.filters.sort = sort;
                this.filters.order = order || 'asc';
                this.loadShelters();
            });
        }

        // Add shelter button
        const addBtn = document.getElementById('add-shelter-btn');
        if (addBtn) {
            addBtn.addEventListener('click', () => this.showAddShelterModal());
        }

        // Export button
        const exportBtn = document.getElementById('export-shelters-btn');
        if (exportBtn) {
            exportBtn.addEventListener('click', () => this.exportShelters());
        }

        // Modal form submission
        const shelterForm = document.getElementById('shelter-form');
        if (shelterForm) {
            shelterForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveShelter();
            });
        }

        // Logo upload
        const logoInput = document.getElementById('shelter-logo-upload');
        if (logoInput) {
            logoInput.addEventListener('change', (e) => this.handleLogoUpload(e));
        }
    },

    /**
     * Load shelters from API
     */
    async loadShelters() {
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

            const response = await AdminAPI.get('/shelters', params);
            this.shelters = response.data || [];
            this.pagination = response.pagination || this.pagination;

            this.renderSheltersTable();
            this.renderPagination();

        } catch (error) {
            console.error('Load shelters error:', error);
            this.renderError('Failed to load shelters');
        }
    },

    /**
     * Load shelter statistics
     */
    async loadShelterStats() {
        try {
            const response = await AdminAPI.get('/dashboard');
            const stats = response.data || {};

            // Update stat cards
            const totalShelters = document.getElementById('total-shelters-count');
            if (totalShelters) {
                totalShelters.textContent = stats.counts?.total_shelters || 0;
            }

            const verifiedShelters = document.getElementById('verified-shelters-count');
            if (verifiedShelters) {
                verifiedShelters.textContent = stats.counts?.verified_shelters || 0;
            }

            const pendingVerification = document.getElementById('pending-verification-count');
            if (pendingVerification) {
                const pending = (stats.counts?.total_shelters || 0) - (stats.counts?.verified_shelters || 0);
                pendingVerification.textContent = pending;
            }

        } catch (error) {
            console.error('Load shelter stats error:', error);
        }
    },

    /**
     * Render shelters table
     */
    renderSheltersTable() {
        const tbody = document.getElementById('shelters-table-body');
        if (!tbody) return;

        if (this.shelters.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="8" class="text-center py-8">
                        <div class="empty-state">
                            <i class="ri-building-line" style="font-size: 48px; color: var(--color-text-light);"></i>
                            <p>No shelters found matching your criteria</p>
                        </div>
                    </td>
                </tr>
            `;
            return;
        }

        tbody.innerHTML = this.shelters.map(shelter => `
            <tr data-id="${shelter.id}">
                <td>
                    <div class="shelter-cell">
                        <img src="${shelter.logo_url || '/static/images/shelter-placeholder.png'}"
                             alt="${AdminUtils.escapeHtml(shelter.name)}"
                             class="shelter-thumbnail"
                             onerror="this.src='/static/images/shelter-placeholder.png'">
                        <div class="shelter-info">
                            <strong>${AdminUtils.escapeHtml(shelter.name)}</strong>
                            <span class="shelter-id">#${shelter.id.substring(0, 8)}</span>
                        </div>
                    </div>
                </td>
                <td>
                    ${AdminUtils.escapeHtml(shelter.city || '')}, ${AdminUtils.escapeHtml(shelter.state || '')}
                </td>
                <td>
                    <a href="mailto:${shelter.email || ''}" class="text-link">
                        ${AdminUtils.escapeHtml(shelter.email || 'N/A')}
                    </a>
                </td>
                <td>
                    ${AdminUtils.escapeHtml(shelter.phone || 'N/A')}
                </td>
                <td>
                    <span class="dog-count ${shelter.dog_count > 0 ? '' : 'zero'}">
                        ${shelter.dog_count || 0} dogs
                    </span>
                    ${shelter.urgent_count > 0 ? `<span class="urgent-indicator">${shelter.urgent_count} urgent</span>` : ''}
                </td>
                <td>
                    ${shelter.verified ?
                        '<span class="badge badge-success"><i class="ri-checkbox-circle-fill"></i> Verified</span>' :
                        '<span class="badge badge-warning"><i class="ri-time-line"></i> Pending</span>'
                    }
                </td>
                <td>
                    ${AdminUtils.formatRelativeTime(shelter.created_at)}
                </td>
                <td>
                    <div class="action-buttons">
                        <button class="action-btn" onclick="SheltersManager.viewShelter('${shelter.id}')" title="View">
                            <i class="ri-eye-line"></i>
                        </button>
                        <button class="action-btn" onclick="SheltersManager.editShelter('${shelter.id}')" title="Edit">
                            <i class="ri-edit-line"></i>
                        </button>
                        ${!shelter.verified ?
                            `<button class="action-btn success" onclick="SheltersManager.verifyShelter('${shelter.id}')" title="Verify">
                                <i class="ri-checkbox-circle-line"></i>
                            </button>` :
                            `<button class="action-btn warning" onclick="SheltersManager.unverifyShelter('${shelter.id}')" title="Unverify">
                                <i class="ri-close-circle-line"></i>
                            </button>`
                        }
                        <button class="action-btn danger" onclick="SheltersManager.confirmDelete('${shelter.id}')" title="Delete">
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
        const container = document.getElementById('shelters-pagination');
        if (!container) return;

        const { page, total_pages, total } = this.pagination;

        if (total_pages <= 1) {
            container.innerHTML = `<span class="pagination-info">Showing ${total} shelters</span>`;
            return;
        }

        let html = `<span class="pagination-info">Page ${page} of ${total_pages} (${total} shelters)</span>`;
        html += '<div class="pagination-buttons">';

        // Previous button
        html += `<button class="pagination-btn" ${page === 1 ? 'disabled' : ''} onclick="SheltersManager.goToPage(${page - 1})">
            <i class="ri-arrow-left-s-line"></i>
        </button>`;

        // Page numbers
        const maxButtons = 5;
        let startPage = Math.max(1, page - Math.floor(maxButtons / 2));
        let endPage = Math.min(total_pages, startPage + maxButtons - 1);

        if (endPage - startPage < maxButtons - 1) {
            startPage = Math.max(1, endPage - maxButtons + 1);
        }

        if (startPage > 1) {
            html += `<button class="pagination-btn" onclick="SheltersManager.goToPage(1)">1</button>`;
            if (startPage > 2) {
                html += '<span class="pagination-ellipsis">...</span>';
            }
        }

        for (let i = startPage; i <= endPage; i++) {
            html += `<button class="pagination-btn ${i === page ? 'active' : ''}" onclick="SheltersManager.goToPage(${i})">${i}</button>`;
        }

        if (endPage < total_pages) {
            if (endPage < total_pages - 1) {
                html += '<span class="pagination-ellipsis">...</span>';
            }
            html += `<button class="pagination-btn" onclick="SheltersManager.goToPage(${total_pages})">${total_pages}</button>`;
        }

        // Next button
        html += `<button class="pagination-btn" ${page === total_pages ? 'disabled' : ''} onclick="SheltersManager.goToPage(${page + 1})">
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
        this.loadShelters();
    },

    /**
     * Load state options for filter
     */
    loadStateOptions() {
        const states = [
            'AL', 'AK', 'AZ', 'AR', 'CA', 'CO', 'CT', 'DE', 'FL', 'GA',
            'HI', 'ID', 'IL', 'IN', 'IA', 'KS', 'KY', 'LA', 'ME', 'MD',
            'MA', 'MI', 'MN', 'MS', 'MO', 'MT', 'NE', 'NV', 'NH', 'NJ',
            'NM', 'NY', 'NC', 'ND', 'OH', 'OK', 'OR', 'PA', 'RI', 'SC',
            'SD', 'TN', 'TX', 'UT', 'VT', 'VA', 'WA', 'WV', 'WI', 'WY'
        ];

        const select = document.getElementById('shelters-state-filter');
        if (select) {
            select.innerHTML = '<option value="">All States</option>' +
                states.map(s => `<option value="${s}">${s}</option>`).join('');
        }

        const formSelect = document.getElementById('shelter-state');
        if (formSelect) {
            formSelect.innerHTML = '<option value="">Select State</option>' +
                states.map(s => `<option value="${s}">${s}</option>`).join('');
        }
    },

    /**
     * View shelter details
     */
    async viewShelter(id) {
        try {
            const response = await AdminAPI.get(`/shelters/${id}`);
            const shelter = response.data;

            this.showShelterDetailModal(shelter);
        } catch (error) {
            console.error('View shelter error:', error);
            AdminToast.error('Error', 'Failed to load shelter details');
        }
    },

    /**
     * Show shelter detail modal
     */
    showShelterDetailModal(shelter) {
        const content = `
            <div class="shelter-detail">
                <div class="shelter-detail-header">
                    <img src="${shelter.logo_url || '/static/images/shelter-placeholder.png'}"
                         alt="${AdminUtils.escapeHtml(shelter.name)}"
                         class="shelter-detail-logo">
                    <div class="shelter-detail-info">
                        <h2>${AdminUtils.escapeHtml(shelter.name)}</h2>
                        <p class="shelter-id">ID: ${shelter.id}</p>
                        ${shelter.verified ?
                            '<span class="badge badge-success"><i class="ri-checkbox-circle-fill"></i> Verified</span>' :
                            '<span class="badge badge-warning"><i class="ri-time-line"></i> Pending Verification</span>'
                        }
                    </div>
                </div>
                <div class="shelter-detail-body">
                    <div class="detail-grid">
                        <div class="detail-item">
                            <label>Address</label>
                            <span>
                                ${AdminUtils.escapeHtml(shelter.address || '')}
                                ${shelter.address_2 ? '<br>' + AdminUtils.escapeHtml(shelter.address_2) : ''}
                                <br>${AdminUtils.escapeHtml(shelter.city || '')}, ${AdminUtils.escapeHtml(shelter.state || '')} ${AdminUtils.escapeHtml(shelter.zip_code || '')}
                            </span>
                        </div>
                        <div class="detail-item">
                            <label>Contact</label>
                            <span>
                                ${shelter.email ? `<a href="mailto:${shelter.email}">${AdminUtils.escapeHtml(shelter.email)}</a><br>` : ''}
                                ${shelter.phone ? AdminUtils.escapeHtml(shelter.phone) : 'No phone'}
                            </span>
                        </div>
                        <div class="detail-item">
                            <label>Website</label>
                            <span>
                                ${shelter.website ?
                                    `<a href="${shelter.website}" target="_blank">${AdminUtils.escapeHtml(shelter.website)}</a>` :
                                    'No website'
                                }
                            </span>
                        </div>
                        <div class="detail-item">
                            <label>Social Media</label>
                            <span>
                                ${shelter.facebook_url ? `<a href="${shelter.facebook_url}" target="_blank"><i class="ri-facebook-fill"></i></a> ` : ''}
                                ${shelter.instagram_url ? `<a href="${shelter.instagram_url}" target="_blank"><i class="ri-instagram-fill"></i></a> ` : ''}
                                ${shelter.twitter_url ? `<a href="${shelter.twitter_url}" target="_blank"><i class="ri-twitter-fill"></i></a>` : ''}
                                ${!shelter.facebook_url && !shelter.instagram_url && !shelter.twitter_url ? 'None' : ''}
                            </span>
                        </div>
                        <div class="detail-item">
                            <label>Total Dogs</label>
                            <span>${shelter.dog_count || 0}</span>
                        </div>
                        <div class="detail-item">
                            <label>Urgent Dogs</label>
                            <span class="${shelter.urgent_count > 0 ? 'text-danger' : ''}">${shelter.urgent_count || 0}</span>
                        </div>
                        <div class="detail-item">
                            <label>Registered</label>
                            <span>${shelter.created_at ? new Date(shelter.created_at).toLocaleDateString() : 'Unknown'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Last Updated</label>
                            <span>${shelter.updated_at ? AdminUtils.formatRelativeTime(shelter.updated_at) : 'Never'}</span>
                        </div>
                    </div>
                    <div class="detail-section">
                        <label>Description</label>
                        <p>${AdminUtils.escapeHtml(shelter.description || 'No description available.')}</p>
                    </div>
                    <div class="detail-section">
                        <label>Operating Hours</label>
                        <p>${AdminUtils.escapeHtml(shelter.operating_hours || 'Not specified')}</p>
                    </div>
                    <div class="detail-section">
                        <label>Adoption Process</label>
                        <p>${AdminUtils.escapeHtml(shelter.adoption_process || 'Not specified')}</p>
                    </div>
                </div>
            </div>
        `;

        AdminModal.show({
            title: `Shelter Details: ${shelter.name}`,
            content: content,
            size: 'large',
            buttons: [
                { text: 'Edit', class: 'btn-primary', action: () => { AdminModal.hide(); this.editShelter(shelter.id); } },
                { text: 'View Dogs', class: 'btn-secondary', action: () => { AdminModal.hide(); this.viewShelterDogs(shelter.id); } },
                { text: 'Close', class: 'btn-secondary', action: () => AdminModal.hide() }
            ]
        });
    },

    /**
     * View dogs for a shelter
     */
    viewShelterDogs(shelterId) {
        // Navigate to dogs page with shelter filter
        AdminNav.navigate('dogs');
        setTimeout(() => {
            DogsManager.filters.shelter_id = shelterId;
            DogsManager.loadDogs();
            const shelterFilter = document.getElementById('dogs-shelter-filter');
            if (shelterFilter) {
                shelterFilter.value = shelterId;
            }
        }, 100);
    },

    /**
     * Show add shelter modal
     */
    showAddShelterModal() {
        this.currentShelter = null;
        this.resetForm();
        document.getElementById('shelter-modal-title').textContent = 'Add New Shelter';
        AdminModal.show({ id: 'shelter-modal' });
    },

    /**
     * Edit shelter
     */
    async editShelter(id) {
        try {
            const response = await AdminAPI.get(`/shelters/${id}`);
            this.currentShelter = response.data;

            this.populateForm(this.currentShelter);
            document.getElementById('shelter-modal-title').textContent = 'Edit Shelter';
            AdminModal.show({ id: 'shelter-modal' });
        } catch (error) {
            console.error('Edit shelter error:', error);
            AdminToast.error('Error', 'Failed to load shelter for editing');
        }
    },

    /**
     * Reset form
     */
    resetForm() {
        const form = document.getElementById('shelter-form');
        if (form) {
            form.reset();
        }
        const preview = document.getElementById('shelter-logo-preview');
        if (preview) {
            preview.innerHTML = '';
        }
    },

    /**
     * Populate form with shelter data
     */
    populateForm(shelter) {
        const fields = ['name', 'email', 'phone', 'website', 'address', 'address_2',
                       'city', 'state', 'zip_code', 'description', 'operating_hours',
                       'adoption_process', 'facebook_url', 'instagram_url', 'twitter_url'];

        fields.forEach(field => {
            const input = document.getElementById(`shelter-${field.replace('_', '-')}`);
            if (input) {
                input.value = shelter[field] || '';
            }
        });

        // Handle verified checkbox
        const verifiedCheckbox = document.getElementById('shelter-verified');
        if (verifiedCheckbox) {
            verifiedCheckbox.checked = shelter.verified || false;
        }

        // Handle logo preview
        const preview = document.getElementById('shelter-logo-preview');
        if (preview && shelter.logo_url) {
            preview.innerHTML = `<img src="${shelter.logo_url}" alt="Current logo" class="logo-preview-img">`;
        }
    },

    /**
     * Save shelter (create or update)
     */
    async saveShelter() {
        try {
            const formData = this.getFormData();

            if (!this.validateForm(formData)) {
                return;
            }

            let response;
            if (this.currentShelter) {
                response = await AdminAPI.put(`/shelters/${this.currentShelter.id}`, formData);
                AdminToast.success('Success', 'Shelter updated successfully');
            } else {
                response = await AdminAPI.post('/shelters', formData);
                AdminToast.success('Success', 'Shelter created successfully');
            }

            AdminModal.hide('shelter-modal');
            this.loadShelters();
        } catch (error) {
            console.error('Save shelter error:', error);
            AdminToast.error('Error', error.message || 'Failed to save shelter');
        }
    },

    /**
     * Get form data
     */
    getFormData() {
        const form = document.getElementById('shelter-form');
        const data = {};

        const textFields = ['name', 'email', 'phone', 'website', 'address', 'address_2',
                           'city', 'state', 'zip_code', 'description', 'operating_hours',
                           'adoption_process', 'facebook_url', 'instagram_url', 'twitter_url'];

        textFields.forEach(field => {
            const input = form.querySelector(`#shelter-${field.replace('_', '-')}`);
            if (input) {
                data[field] = input.value.trim();
            }
        });

        // Handle verified checkbox
        const verifiedCheckbox = form.querySelector('#shelter-verified');
        if (verifiedCheckbox) {
            data.verified = verifiedCheckbox.checked;
        }

        return data;
    },

    /**
     * Validate form
     */
    validateForm(data) {
        if (!data.name) {
            AdminToast.error('Validation Error', 'Shelter name is required');
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
        return true;
    },

    /**
     * Handle logo upload
     */
    async handleLogoUpload(event) {
        const file = event.target.files[0];
        if (!file) return;

        if (!file.type.startsWith('image/')) {
            AdminToast.error('Error', 'Please select an image file');
            return;
        }

        if (file.size > 2 * 1024 * 1024) {
            AdminToast.error('Error', 'Logo must be less than 2MB');
            return;
        }

        const preview = document.getElementById('shelter-logo-preview');
        if (preview) {
            const reader = new FileReader();
            reader.onload = (e) => {
                preview.innerHTML = `<img src="${e.target.result}" alt="Preview" class="logo-preview-img">`;
            };
            reader.readAsDataURL(file);
        }

        if (this.currentShelter) {
            try {
                const formData = new FormData();
                formData.append('logo', file);

                AdminToast.info('Uploading', 'Uploading logo...');

                const response = await fetch(`${AdminConfig.apiBase}/shelters/${this.currentShelter.id}/logo`, {
                    method: 'POST',
                    headers: {
                        'Authorization': `Bearer ${localStorage.getItem('access_token')}`
                    },
                    body: formData
                });

                if (!response.ok) {
                    throw new Error('Upload failed');
                }

                AdminToast.success('Success', 'Logo uploaded successfully');
            } catch (error) {
                console.error('Logo upload error:', error);
                AdminToast.error('Error', 'Failed to upload logo');
            }
        }
    },

    /**
     * Verify shelter
     */
    async verifyShelter(id) {
        try {
            await AdminAPI.put(`/shelters/${id}`, { verified: true });
            AdminToast.success('Success', 'Shelter verified successfully');
            this.loadShelters();
        } catch (error) {
            console.error('Verify shelter error:', error);
            AdminToast.error('Error', 'Failed to verify shelter');
        }
    },

    /**
     * Unverify shelter
     */
    async unverifyShelter(id) {
        AdminModal.confirm({
            title: 'Unverify Shelter',
            message: 'Are you sure you want to remove verification from this shelter?',
            confirmText: 'Unverify',
            confirmClass: 'btn-warning',
            onConfirm: async () => {
                try {
                    await AdminAPI.put(`/shelters/${id}`, { verified: false });
                    AdminToast.success('Success', 'Shelter unverified');
                    this.loadShelters();
                } catch (error) {
                    console.error('Unverify shelter error:', error);
                    AdminToast.error('Error', 'Failed to unverify shelter');
                }
            }
        });
    },

    /**
     * Confirm delete shelter
     */
    confirmDelete(id) {
        const shelter = this.shelters.find(s => s.id === id);
        if (!shelter) return;

        let message = `Are you sure you want to delete "${shelter.name}"?`;
        if (shelter.dog_count > 0) {
            message += ` This shelter has ${shelter.dog_count} dog(s) associated with it.`;
        }
        message += ' This action cannot be undone.';

        AdminModal.confirm({
            title: 'Delete Shelter',
            message: message,
            confirmText: 'Delete',
            confirmClass: 'btn-danger',
            onConfirm: () => this.deleteShelter(id)
        });
    },

    /**
     * Delete shelter
     */
    async deleteShelter(id) {
        try {
            await AdminAPI.delete(`/shelters/${id}`);
            AdminToast.success('Success', 'Shelter deleted successfully');
            this.loadShelters();
        } catch (error) {
            console.error('Delete shelter error:', error);
            AdminToast.error('Error', 'Failed to delete shelter');
        }
    },

    /**
     * Export shelters
     */
    async exportShelters() {
        try {
            AdminToast.info('Exporting', 'Preparing export...');

            const params = { ...this.filters, per_page: 10000 };
            const response = await AdminAPI.get('/shelters', params);
            const shelters = response.data || [];

            if (shelters.length === 0) {
                AdminToast.warning('Warning', 'No shelters to export');
                return;
            }

            const headers = ['ID', 'Name', 'City', 'State', 'Email', 'Phone', 'Dogs', 'Verified', 'Created'];
            const rows = shelters.map(shelter => [
                shelter.id,
                shelter.name,
                shelter.city || '',
                shelter.state || '',
                shelter.email || '',
                shelter.phone || '',
                shelter.dog_count || 0,
                shelter.verified ? 'Yes' : 'No',
                shelter.created_at || ''
            ]);

            const csv = [headers, ...rows].map(row => row.map(cell => `"${String(cell).replace(/"/g, '""')}"`).join(',')).join('\n');

            const blob = new Blob([csv], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `shelters-export-${new Date().toISOString().split('T')[0]}.csv`;
            a.click();
            URL.revokeObjectURL(url);

            AdminToast.success('Success', `Exported ${shelters.length} shelters`);
        } catch (error) {
            console.error('Export error:', error);
            AdminToast.error('Error', 'Failed to export shelters');
        }
    },

    /**
     * Render error state
     */
    renderError(message) {
        const tbody = document.getElementById('shelters-table-body');
        if (tbody) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="8" class="text-center py-8">
                        <div class="error-state">
                            <i class="ri-error-warning-line" style="font-size: 48px; color: var(--color-danger);"></i>
                            <p>${AdminUtils.escapeHtml(message)}</p>
                            <button class="btn btn-primary" onclick="SheltersManager.loadShelters()">Try Again</button>
                        </div>
                    </td>
                </tr>
            `;
        }
    }
};

// Add email validation to AdminUtils if not exists
if (typeof AdminUtils !== 'undefined' && !AdminUtils.isValidEmail) {
    AdminUtils.isValidEmail = function(email) {
        const re = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        return re.test(email);
    };
}

// Export
window.SheltersManager = SheltersManager;
