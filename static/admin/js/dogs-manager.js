/**
 * Dogs Manager JavaScript
 * WaitingTheLongest.com Admin Dashboard
 * Author: Agent 20
 *
 * Handles dog management: list, search, filter, edit, upload photos, update status
 */

const DogsManager = {
    dogs: [],
    currentDog: null,
    filters: {
        search: '',
        status: '',
        urgency: '',
        shelter_id: '',
        breed: '',
        size: '',
        age_range: '',
        sort: 'urgency_level',
        order: 'desc'
    },
    pagination: {
        page: 1,
        per_page: 20,
        total: 0,
        total_pages: 0
    },

    /**
     * Initialize dogs manager
     */
    init() {
        this.bindEvents();
        this.loadShelterOptions();
    },

    /**
     * Load dogs page
     */
    async load() {
        try {
            await this.loadDogs();
        } catch (error) {
            console.error('Dogs load error:', error);
            AdminToast.error('Error', 'Failed to load dogs');
        }
    },

    /**
     * Bind event listeners
     */
    bindEvents() {
        // Search input
        const searchInput = document.getElementById('dogs-search');
        if (searchInput) {
            let debounceTimer;
            searchInput.addEventListener('input', (e) => {
                clearTimeout(debounceTimer);
                debounceTimer = setTimeout(() => {
                    this.filters.search = e.target.value;
                    this.pagination.page = 1;
                    this.loadDogs();
                }, 300);
            });
        }

        // Filter selects
        const filterIds = ['dogs-status-filter', 'dogs-urgency-filter', 'dogs-shelter-filter',
                          'dogs-breed-filter', 'dogs-size-filter', 'dogs-age-filter'];
        filterIds.forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.addEventListener('change', (e) => {
                    const filterKey = id.replace('dogs-', '').replace('-filter', '').replace('-', '_');
                    this.filters[filterKey] = e.target.value;
                    this.pagination.page = 1;
                    this.loadDogs();
                });
            }
        });

        // Sort select
        const sortSelect = document.getElementById('dogs-sort');
        if (sortSelect) {
            sortSelect.addEventListener('change', (e) => {
                const [sort, order] = e.target.value.split(':');
                this.filters.sort = sort;
                this.filters.order = order || 'asc';
                this.loadDogs();
            });
        }

        // Select all checkbox
        const selectAll = document.getElementById('dogs-select-all');
        if (selectAll) {
            selectAll.addEventListener('change', (e) => {
                const checkboxes = document.querySelectorAll('.dog-checkbox');
                checkboxes.forEach(cb => cb.checked = e.target.checked);
                this.updateBulkActions();
            });
        }

        // Bulk action button
        const bulkBtn = document.getElementById('dogs-bulk-action-btn');
        if (bulkBtn) {
            bulkBtn.addEventListener('click', () => this.executeBulkAction());
        }

        // Add dog button
        const addBtn = document.getElementById('add-dog-btn');
        if (addBtn) {
            addBtn.addEventListener('click', () => this.showAddDogModal());
        }

        // Export button
        const exportBtn = document.getElementById('export-dogs-btn');
        if (exportBtn) {
            exportBtn.addEventListener('click', () => this.exportDogs());
        }

        // Modal form submission
        const dogForm = document.getElementById('dog-form');
        if (dogForm) {
            dogForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveDog();
            });
        }

        // Photo upload
        const photoInput = document.getElementById('dog-photo-upload');
        if (photoInput) {
            photoInput.addEventListener('change', (e) => this.handlePhotoUpload(e));
        }
    },

    /**
     * Load dogs from API
     */
    async loadDogs() {
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

            const response = await AdminAPI.get('/dogs', params);
            this.dogs = response.data || [];
            this.pagination = response.pagination || this.pagination;

            this.renderDogsTable();
            this.renderPagination();

        } catch (error) {
            console.error('Load dogs error:', error);
            this.renderError('Failed to load dogs');
        }
    },

    /**
     * Render dogs table
     */
    renderDogsTable() {
        const tbody = document.getElementById('dogs-table-body');
        if (!tbody) return;

        if (this.dogs.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="9" class="text-center py-8">
                        <div class="empty-state">
                            <i class="ri-emotion-sad-line" style="font-size: 48px; color: var(--color-text-light);"></i>
                            <p>No dogs found matching your criteria</p>
                        </div>
                    </td>
                </tr>
            `;
            return;
        }

        tbody.innerHTML = this.dogs.map(dog => `
            <tr data-id="${dog.id}">
                <td>
                    <input type="checkbox" class="dog-checkbox" value="${dog.id}" onchange="DogsManager.updateBulkActions()">
                </td>
                <td>
                    <div class="dog-cell">
                        <img src="${dog.photo_url || '/static/images/dog-placeholder.png'}"
                             alt="${AdminUtils.escapeHtml(dog.name)}"
                             class="dog-thumbnail"
                             onerror="this.src='/static/images/dog-placeholder.png'">
                        <div class="dog-info">
                            <strong>${AdminUtils.escapeHtml(dog.name)}</strong>
                            <span class="dog-id">#${dog.id.substring(0, 8)}</span>
                        </div>
                    </div>
                </td>
                <td>${AdminUtils.escapeHtml(dog.breed || 'Unknown')}</td>
                <td>${dog.age_years || 0}y ${dog.age_months || 0}m</td>
                <td>
                    <span class="urgency-badge urgency-${dog.urgency_level?.toLowerCase() || 'normal'}">
                        ${dog.urgency_level || 'Normal'}
                    </span>
                </td>
                <td>
                    <span class="status-badge status-${dog.status?.toLowerCase() || 'available'}">
                        ${dog.status || 'Available'}
                    </span>
                </td>
                <td>${AdminUtils.escapeHtml(dog.shelter_name || 'Unknown')}</td>
                <td>
                    <div class="wait-time" data-intake="${dog.intake_date}">
                        ${this.formatWaitTime(dog.intake_date)}
                    </div>
                </td>
                <td>
                    <div class="action-buttons">
                        <button class="action-btn" onclick="DogsManager.viewDog('${dog.id}')" title="View">
                            <i class="ri-eye-line"></i>
                        </button>
                        <button class="action-btn" onclick="DogsManager.editDog('${dog.id}')" title="Edit">
                            <i class="ri-edit-line"></i>
                        </button>
                        <button class="action-btn" onclick="DogsManager.showStatusModal('${dog.id}')" title="Update Status">
                            <i class="ri-refresh-line"></i>
                        </button>
                        <button class="action-btn danger" onclick="DogsManager.confirmDelete('${dog.id}')" title="Delete">
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
        const container = document.getElementById('dogs-pagination');
        if (!container) return;

        const { page, total_pages, total } = this.pagination;

        if (total_pages <= 1) {
            container.innerHTML = `<span class="pagination-info">Showing ${total} dogs</span>`;
            return;
        }

        let html = `<span class="pagination-info">Page ${page} of ${total_pages} (${total} dogs)</span>`;
        html += '<div class="pagination-buttons">';

        // Previous button
        html += `<button class="pagination-btn" ${page === 1 ? 'disabled' : ''} onclick="DogsManager.goToPage(${page - 1})">
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
            html += `<button class="pagination-btn" onclick="DogsManager.goToPage(1)">1</button>`;
            if (startPage > 2) {
                html += '<span class="pagination-ellipsis">...</span>';
            }
        }

        for (let i = startPage; i <= endPage; i++) {
            html += `<button class="pagination-btn ${i === page ? 'active' : ''}" onclick="DogsManager.goToPage(${i})">${i}</button>`;
        }

        if (endPage < total_pages) {
            if (endPage < total_pages - 1) {
                html += '<span class="pagination-ellipsis">...</span>';
            }
            html += `<button class="pagination-btn" onclick="DogsManager.goToPage(${total_pages})">${total_pages}</button>`;
        }

        // Next button
        html += `<button class="pagination-btn" ${page === total_pages ? 'disabled' : ''} onclick="DogsManager.goToPage(${page + 1})">
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
        this.loadDogs();
    },

    /**
     * Format wait time
     */
    formatWaitTime(intakeDate) {
        if (!intakeDate) return 'Unknown';

        const intake = new Date(intakeDate);
        const now = new Date();
        const diffMs = now - intake;
        const diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24));

        if (diffDays === 0) return 'Today';
        if (diffDays === 1) return '1 day';
        if (diffDays < 30) return `${diffDays} days`;
        if (diffDays < 365) {
            const months = Math.floor(diffDays / 30);
            return `${months} month${months > 1 ? 's' : ''}`;
        }
        const years = Math.floor(diffDays / 365);
        return `${years} year${years > 1 ? 's' : ''}`;
    },

    /**
     * Load shelter options for filter
     */
    async loadShelterOptions() {
        try {
            const response = await AdminAPI.get('/shelters', { per_page: 1000 });
            const shelters = response.data || [];

            const select = document.getElementById('dogs-shelter-filter');
            if (select) {
                select.innerHTML = '<option value="">All Shelters</option>' +
                    shelters.map(s => `<option value="${s.id}">${AdminUtils.escapeHtml(s.name)}</option>`).join('');
            }

            // Also populate the dog form shelter select
            const formSelect = document.getElementById('dog-shelter-id');
            if (formSelect) {
                formSelect.innerHTML = '<option value="">Select Shelter</option>' +
                    shelters.map(s => `<option value="${s.id}">${AdminUtils.escapeHtml(s.name)}</option>`).join('');
            }
        } catch (error) {
            console.error('Load shelters error:', error);
        }
    },

    /**
     * View dog details
     */
    async viewDog(id) {
        try {
            const response = await AdminAPI.get(`/dogs/${id}`);
            const dog = response.data;

            // Show view modal or navigate to detail page
            this.showDogDetailModal(dog);
        } catch (error) {
            console.error('View dog error:', error);
            AdminToast.error('Error', 'Failed to load dog details');
        }
    },

    /**
     * Show dog detail modal
     */
    showDogDetailModal(dog) {
        const content = `
            <div class="dog-detail">
                <div class="dog-detail-header">
                    <img src="${dog.photo_url || '/static/images/dog-placeholder.png'}"
                         alt="${AdminUtils.escapeHtml(dog.name)}"
                         class="dog-detail-image">
                    <div class="dog-detail-info">
                        <h2>${AdminUtils.escapeHtml(dog.name)}</h2>
                        <p class="dog-id">ID: ${dog.id}</p>
                        <span class="urgency-badge urgency-${dog.urgency_level?.toLowerCase() || 'normal'}">
                            ${dog.urgency_level || 'Normal'}
                        </span>
                        <span class="status-badge status-${dog.status?.toLowerCase() || 'available'}">
                            ${dog.status || 'Available'}
                        </span>
                    </div>
                </div>
                <div class="dog-detail-body">
                    <div class="detail-grid">
                        <div class="detail-item">
                            <label>Breed</label>
                            <span>${AdminUtils.escapeHtml(dog.breed || 'Unknown')}</span>
                        </div>
                        <div class="detail-item">
                            <label>Age</label>
                            <span>${dog.age_years || 0} years, ${dog.age_months || 0} months</span>
                        </div>
                        <div class="detail-item">
                            <label>Size</label>
                            <span>${dog.size || 'Unknown'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Weight</label>
                            <span>${dog.weight ? dog.weight + ' lbs' : 'Unknown'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Gender</label>
                            <span>${dog.gender || 'Unknown'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Color</label>
                            <span>${AdminUtils.escapeHtml(dog.color || 'Unknown')}</span>
                        </div>
                        <div class="detail-item">
                            <label>Shelter</label>
                            <span>${AdminUtils.escapeHtml(dog.shelter_name || 'Unknown')}</span>
                        </div>
                        <div class="detail-item">
                            <label>Intake Date</label>
                            <span>${dog.intake_date ? new Date(dog.intake_date).toLocaleDateString() : 'Unknown'}</span>
                        </div>
                        <div class="detail-item">
                            <label>Wait Time</label>
                            <span>${this.formatWaitTime(dog.intake_date)}</span>
                        </div>
                        <div class="detail-item">
                            <label>Euthanasia Date</label>
                            <span class="${dog.euthanasia_date ? 'text-danger' : ''}">
                                ${dog.euthanasia_date ? new Date(dog.euthanasia_date).toLocaleDateString() : 'Not scheduled'}
                            </span>
                        </div>
                    </div>
                    <div class="detail-section">
                        <label>Description</label>
                        <p>${AdminUtils.escapeHtml(dog.description || 'No description available.')}</p>
                    </div>
                    <div class="detail-section">
                        <label>Temperament</label>
                        <div class="tag-list">
                            ${(dog.temperament || []).map(t => `<span class="tag">${AdminUtils.escapeHtml(t)}</span>`).join('') || 'Not specified'}
                        </div>
                    </div>
                    <div class="detail-section">
                        <label>Medical Notes</label>
                        <p>${AdminUtils.escapeHtml(dog.medical_notes || 'No medical notes.')}</p>
                    </div>
                </div>
            </div>
        `;

        AdminModal.show({
            title: `Dog Details: ${dog.name}`,
            content: content,
            size: 'large',
            buttons: [
                { text: 'Edit', class: 'btn-primary', action: () => { AdminModal.hide(); this.editDog(dog.id); } },
                { text: 'Close', class: 'btn-secondary', action: () => AdminModal.hide() }
            ]
        });
    },

    /**
     * Show add dog modal
     */
    showAddDogModal() {
        this.currentDog = null;
        this.resetForm();
        document.getElementById('dog-modal-title').textContent = 'Add New Dog';
        AdminModal.show({ id: 'dog-modal' });
    },

    /**
     * Edit dog
     */
    async editDog(id) {
        try {
            const response = await AdminAPI.get(`/dogs/${id}`);
            this.currentDog = response.data;

            this.populateForm(this.currentDog);
            document.getElementById('dog-modal-title').textContent = 'Edit Dog';
            AdminModal.show({ id: 'dog-modal' });
        } catch (error) {
            console.error('Edit dog error:', error);
            AdminToast.error('Error', 'Failed to load dog for editing');
        }
    },

    /**
     * Reset form
     */
    resetForm() {
        const form = document.getElementById('dog-form');
        if (form) {
            form.reset();
        }
        // Clear photo preview
        const preview = document.getElementById('dog-photo-preview');
        if (preview) {
            preview.innerHTML = '';
        }
    },

    /**
     * Populate form with dog data
     */
    populateForm(dog) {
        const fields = ['name', 'breed', 'age_years', 'age_months', 'size', 'weight',
                       'gender', 'color', 'status', 'description', 'medical_notes', 'shelter_id'];

        fields.forEach(field => {
            const input = document.getElementById(`dog-${field.replace('_', '-')}`);
            if (input) {
                input.value = dog[field] || '';
            }
        });

        // Handle intake date
        const intakeDate = document.getElementById('dog-intake-date');
        if (intakeDate && dog.intake_date) {
            intakeDate.value = dog.intake_date.split('T')[0];
        }

        // Handle euthanasia date
        const euthDate = document.getElementById('dog-euthanasia-date');
        if (euthDate && dog.euthanasia_date) {
            euthDate.value = dog.euthanasia_date.split('T')[0];
        }

        // Handle temperament (array)
        const tempInput = document.getElementById('dog-temperament');
        if (tempInput && dog.temperament) {
            tempInput.value = Array.isArray(dog.temperament) ? dog.temperament.join(', ') : dog.temperament;
        }

        // Handle photo preview
        const preview = document.getElementById('dog-photo-preview');
        if (preview && dog.photo_url) {
            preview.innerHTML = `<img src="${dog.photo_url}" alt="Current photo" class="photo-preview-img">`;
        }
    },

    /**
     * Save dog (create or update)
     */
    async saveDog() {
        try {
            const formData = this.getFormData();

            if (!this.validateForm(formData)) {
                return;
            }

            let response;
            if (this.currentDog) {
                response = await AdminAPI.put(`/dogs/${this.currentDog.id}`, formData);
                AdminToast.success('Success', 'Dog updated successfully');
            } else {
                response = await AdminAPI.post('/dogs', formData);
                AdminToast.success('Success', 'Dog created successfully');
            }

            AdminModal.hide('dog-modal');
            this.loadDogs();
        } catch (error) {
            console.error('Save dog error:', error);
            AdminToast.error('Error', error.message || 'Failed to save dog');
        }
    },

    /**
     * Get form data
     */
    getFormData() {
        const form = document.getElementById('dog-form');
        const data = {};

        // Text fields
        const textFields = ['name', 'breed', 'size', 'color', 'status', 'description',
                           'medical_notes', 'shelter_id', 'gender'];
        textFields.forEach(field => {
            const input = form.querySelector(`#dog-${field.replace('_', '-')}`);
            if (input) {
                data[field] = input.value.trim();
            }
        });

        // Number fields
        const numFields = ['age_years', 'age_months', 'weight'];
        numFields.forEach(field => {
            const input = form.querySelector(`#dog-${field.replace('_', '-')}`);
            if (input && input.value) {
                data[field] = parseInt(input.value, 10);
            }
        });

        // Date fields
        const dateFields = ['intake_date', 'euthanasia_date'];
        dateFields.forEach(field => {
            const input = form.querySelector(`#dog-${field.replace('_', '-')}`);
            if (input && input.value) {
                data[field] = input.value;
            }
        });

        // Temperament (comma-separated to array)
        const tempInput = form.querySelector('#dog-temperament');
        if (tempInput && tempInput.value) {
            data.temperament = tempInput.value.split(',').map(t => t.trim()).filter(t => t);
        }

        return data;
    },

    /**
     * Validate form
     */
    validateForm(data) {
        if (!data.name) {
            AdminToast.error('Validation Error', 'Dog name is required');
            return false;
        }
        if (!data.shelter_id) {
            AdminToast.error('Validation Error', 'Shelter is required');
            return false;
        }
        return true;
    },

    /**
     * Handle photo upload
     */
    async handlePhotoUpload(event) {
        const file = event.target.files[0];
        if (!file) return;

        // Validate file type
        if (!file.type.startsWith('image/')) {
            AdminToast.error('Error', 'Please select an image file');
            return;
        }

        // Validate file size (max 5MB)
        if (file.size > 5 * 1024 * 1024) {
            AdminToast.error('Error', 'Image must be less than 5MB');
            return;
        }

        // Show preview
        const preview = document.getElementById('dog-photo-preview');
        if (preview) {
            const reader = new FileReader();
            reader.onload = (e) => {
                preview.innerHTML = `<img src="${e.target.result}" alt="Preview" class="photo-preview-img">`;
            };
            reader.readAsDataURL(file);
        }

        // Upload if editing existing dog
        if (this.currentDog) {
            try {
                const formData = new FormData();
                formData.append('photo', file);

                AdminToast.info('Uploading', 'Uploading photo...');

                const response = await fetch(`${AdminConfig.apiBase}/dogs/${this.currentDog.id}/photo`, {
                    method: 'POST',
                    headers: {
                        'Authorization': `Bearer ${localStorage.getItem('access_token')}`
                    },
                    body: formData
                });

                if (!response.ok) {
                    throw new Error('Upload failed');
                }

                AdminToast.success('Success', 'Photo uploaded successfully');
            } catch (error) {
                console.error('Photo upload error:', error);
                AdminToast.error('Error', 'Failed to upload photo');
            }
        }
    },

    /**
     * Show status update modal
     */
    showStatusModal(id) {
        const dog = this.dogs.find(d => d.id === id);
        if (!dog) return;

        const content = `
            <form id="status-form">
                <div class="form-group">
                    <label for="new-status">New Status</label>
                    <select id="new-status" class="form-control" required>
                        <option value="available" ${dog.status === 'available' ? 'selected' : ''}>Available</option>
                        <option value="pending" ${dog.status === 'pending' ? 'selected' : ''}>Pending</option>
                        <option value="adopted" ${dog.status === 'adopted' ? 'selected' : ''}>Adopted</option>
                        <option value="fostered" ${dog.status === 'fostered' ? 'selected' : ''}>Fostered</option>
                        <option value="on_hold" ${dog.status === 'on_hold' ? 'selected' : ''}>On Hold</option>
                        <option value="medical_hold" ${dog.status === 'medical_hold' ? 'selected' : ''}>Medical Hold</option>
                        <option value="unavailable" ${dog.status === 'unavailable' ? 'selected' : ''}>Unavailable</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="status-notes">Notes (optional)</label>
                    <textarea id="status-notes" class="form-control" rows="3" placeholder="Reason for status change..."></textarea>
                </div>
            </form>
        `;

        AdminModal.show({
            title: `Update Status: ${dog.name}`,
            content: content,
            buttons: [
                { text: 'Cancel', class: 'btn-secondary', action: () => AdminModal.hide() },
                { text: 'Update Status', class: 'btn-primary', action: () => this.updateStatus(id) }
            ]
        });
    },

    /**
     * Update dog status
     */
    async updateStatus(id) {
        try {
            const status = document.getElementById('new-status').value;
            const notes = document.getElementById('status-notes').value;

            await AdminAPI.put(`/dogs/${id}`, { status, status_notes: notes });

            AdminToast.success('Success', 'Status updated successfully');
            AdminModal.hide();
            this.loadDogs();
        } catch (error) {
            console.error('Update status error:', error);
            AdminToast.error('Error', 'Failed to update status');
        }
    },

    /**
     * Confirm delete dog
     */
    confirmDelete(id) {
        const dog = this.dogs.find(d => d.id === id);
        if (!dog) return;

        AdminModal.confirm({
            title: 'Delete Dog',
            message: `Are you sure you want to delete "${dog.name}"? This action cannot be undone.`,
            confirmText: 'Delete',
            confirmClass: 'btn-danger',
            onConfirm: () => this.deleteDog(id)
        });
    },

    /**
     * Delete dog
     */
    async deleteDog(id) {
        try {
            await AdminAPI.delete(`/dogs/${id}`);
            AdminToast.success('Success', 'Dog deleted successfully');
            this.loadDogs();
        } catch (error) {
            console.error('Delete dog error:', error);
            AdminToast.error('Error', 'Failed to delete dog');
        }
    },

    /**
     * Update bulk actions visibility
     */
    updateBulkActions() {
        const checkboxes = document.querySelectorAll('.dog-checkbox:checked');
        const bulkActions = document.getElementById('dogs-bulk-actions');
        const selectedCount = document.getElementById('dogs-selected-count');

        if (bulkActions) {
            bulkActions.style.display = checkboxes.length > 0 ? 'flex' : 'none';
        }
        if (selectedCount) {
            selectedCount.textContent = checkboxes.length;
        }
    },

    /**
     * Get selected dog IDs
     */
    getSelectedIds() {
        const checkboxes = document.querySelectorAll('.dog-checkbox:checked');
        return Array.from(checkboxes).map(cb => cb.value);
    },

    /**
     * Execute bulk action
     */
    async executeBulkAction() {
        const action = document.getElementById('dogs-bulk-action').value;
        const ids = this.getSelectedIds();

        if (!action || ids.length === 0) {
            AdminToast.warning('Warning', 'Please select an action and at least one dog');
            return;
        }

        try {
            switch (action) {
                case 'delete':
                    AdminModal.confirm({
                        title: 'Delete Dogs',
                        message: `Are you sure you want to delete ${ids.length} dog(s)? This action cannot be undone.`,
                        confirmText: 'Delete All',
                        confirmClass: 'btn-danger',
                        onConfirm: async () => {
                            for (const id of ids) {
                                await AdminAPI.delete(`/dogs/${id}`);
                            }
                            AdminToast.success('Success', `Deleted ${ids.length} dog(s)`);
                            this.loadDogs();
                        }
                    });
                    break;

                case 'update_status':
                    this.showBulkStatusModal(ids);
                    break;

                case 'export':
                    this.exportSelected(ids);
                    break;

                default:
                    AdminToast.warning('Warning', 'Unknown action');
            }
        } catch (error) {
            console.error('Bulk action error:', error);
            AdminToast.error('Error', 'Failed to execute bulk action');
        }
    },

    /**
     * Show bulk status update modal
     */
    showBulkStatusModal(ids) {
        const content = `
            <form id="bulk-status-form">
                <p>Update status for ${ids.length} dog(s)</p>
                <div class="form-group">
                    <label for="bulk-new-status">New Status</label>
                    <select id="bulk-new-status" class="form-control" required>
                        <option value="available">Available</option>
                        <option value="pending">Pending</option>
                        <option value="on_hold">On Hold</option>
                        <option value="medical_hold">Medical Hold</option>
                        <option value="unavailable">Unavailable</option>
                    </select>
                </div>
            </form>
        `;

        AdminModal.show({
            title: 'Bulk Status Update',
            content: content,
            buttons: [
                { text: 'Cancel', class: 'btn-secondary', action: () => AdminModal.hide() },
                { text: 'Update All', class: 'btn-primary', action: async () => {
                    const status = document.getElementById('bulk-new-status').value;
                    for (const id of ids) {
                        await AdminAPI.put(`/dogs/${id}`, { status });
                    }
                    AdminToast.success('Success', `Updated ${ids.length} dog(s)`);
                    AdminModal.hide();
                    this.loadDogs();
                }}
            ]
        });
    },

    /**
     * Export dogs
     */
    async exportDogs() {
        try {
            AdminToast.info('Exporting', 'Preparing export...');

            const params = { ...this.filters, per_page: 10000 };
            const response = await AdminAPI.get('/dogs', params);
            const dogs = response.data || [];

            if (dogs.length === 0) {
                AdminToast.warning('Warning', 'No dogs to export');
                return;
            }

            // Generate CSV
            const headers = ['ID', 'Name', 'Breed', 'Age', 'Size', 'Status', 'Urgency', 'Shelter', 'Intake Date', 'Wait Time'];
            const rows = dogs.map(dog => [
                dog.id,
                dog.name,
                dog.breed || '',
                `${dog.age_years || 0}y ${dog.age_months || 0}m`,
                dog.size || '',
                dog.status || '',
                dog.urgency_level || '',
                dog.shelter_name || '',
                dog.intake_date || '',
                this.formatWaitTime(dog.intake_date)
            ]);

            const csv = [headers, ...rows].map(row => row.map(cell => `"${String(cell).replace(/"/g, '""')}"`).join(',')).join('\n');

            // Download
            const blob = new Blob([csv], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `dogs-export-${new Date().toISOString().split('T')[0]}.csv`;
            a.click();
            URL.revokeObjectURL(url);

            AdminToast.success('Success', `Exported ${dogs.length} dogs`);
        } catch (error) {
            console.error('Export error:', error);
            AdminToast.error('Error', 'Failed to export dogs');
        }
    },

    /**
     * Export selected dogs
     */
    async exportSelected(ids) {
        try {
            const dogs = this.dogs.filter(d => ids.includes(d.id));

            if (dogs.length === 0) {
                AdminToast.warning('Warning', 'No dogs selected for export');
                return;
            }

            const headers = ['ID', 'Name', 'Breed', 'Age', 'Size', 'Status', 'Urgency', 'Shelter'];
            const rows = dogs.map(dog => [
                dog.id,
                dog.name,
                dog.breed || '',
                `${dog.age_years || 0}y ${dog.age_months || 0}m`,
                dog.size || '',
                dog.status || '',
                dog.urgency_level || '',
                dog.shelter_name || ''
            ]);

            const csv = [headers, ...rows].map(row => row.map(cell => `"${String(cell).replace(/"/g, '""')}"`).join(',')).join('\n');

            const blob = new Blob([csv], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `dogs-selected-${new Date().toISOString().split('T')[0]}.csv`;
            a.click();
            URL.revokeObjectURL(url);

            AdminToast.success('Success', `Exported ${dogs.length} dogs`);
        } catch (error) {
            console.error('Export selected error:', error);
            AdminToast.error('Error', 'Failed to export selected dogs');
        }
    },

    /**
     * Render error state
     */
    renderError(message) {
        const tbody = document.getElementById('dogs-table-body');
        if (tbody) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="9" class="text-center py-8">
                        <div class="error-state">
                            <i class="ri-error-warning-line" style="font-size: 48px; color: var(--color-danger);"></i>
                            <p>${AdminUtils.escapeHtml(message)}</p>
                            <button class="btn btn-primary" onclick="DogsManager.loadDogs()">Try Again</button>
                        </div>
                    </td>
                </tr>
            `;
        }
    }
};

// Export
window.DogsManager = DogsManager;
