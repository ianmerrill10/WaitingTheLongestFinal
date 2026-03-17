/**
 * @file search.js
 * @brief Global search functionality with auto-suggest
 * PURPOSE: Provides search box auto-suggest and navigation across all pages
 * USAGE: Initialize with SearchManager.init(), attach to search input elements
 * DEPENDENCIES: None (vanilla JavaScript)
 * @author Frontend Build - WaitingTheLongest Build System
 * @date 2024-01-28
 */

class SearchManager {
  constructor() {
    this.suggestionsContainer = null;
    this.searchInput = null;
    this.debounceTimer = null;
    this.debounceDelay = 300;
    this.selectedIndex = -1;
    this.suggestions = [];
    this.isOpen = false;
  }

  /**
   * Initialize search functionality
   * @param {string} inputSelector - CSS selector for search input element
   * @param {string} containerSelector - CSS selector for suggestions container
   */
  static init(inputSelector = '#search-input', containerSelector = '#search-suggestions') {
    if (SearchManager.instance) {
      return SearchManager.instance;
    }

    SearchManager.instance = new SearchManager();
    SearchManager.instance.setup(inputSelector, containerSelector);
    return SearchManager.instance;
  }

  /**
   * Setup event listeners and DOM references
   */
  setup(inputSelector, containerSelector) {
    this.searchInput = document.querySelector(inputSelector);
    this.suggestionsContainer = document.querySelector(containerSelector);

    if (!this.searchInput || !this.suggestionsContainer) {
      console.warn('SearchManager: Input or container not found in DOM');
      return;
    }

    // Input event listener for auto-suggest
    this.searchInput.addEventListener('input', (e) => {
      this.handleInput(e.target.value);
    });

    // Keyboard navigation
    this.searchInput.addEventListener('keydown', (e) => {
      this.handleKeydown(e);
    });

    // Click outside to close suggestions
    document.addEventListener('click', (e) => {
      if (e.target !== this.searchInput && !this.suggestionsContainer.contains(e.target)) {
        this.closeSuggestions();
      }
    });
  }

  /**
   * Handle input events with debouncing
   */
  handleInput(query) {
    clearTimeout(this.debounceTimer);

    if (query.trim().length === 0) {
      this.closeSuggestions();
      return;
    }

    this.debounceTimer = setTimeout(() => {
      this.fetchSuggestions(query);
    }, this.debounceDelay);
  }

  /**
   * Fetch suggestions from API
   */
  async fetchSuggestions(query) {
    try {
      const response = await fetch(
        `/api/search/autocomplete?q=${encodeURIComponent(query)}&limit=5`
      );

      if (!response.ok) {
        throw new Error(`API error: ${response.status}`);
      }

      const data = await response.json();
      this.suggestions = data.suggestions || [];
      this.selectedIndex = -1;
      this.renderSuggestions();
      this.openSuggestions();
    } catch (error) {
      console.error('SearchManager: Error fetching suggestions', error);
      this.closeSuggestions();
    }
  }

  /**
   * Render suggestions dropdown
   */
  renderSuggestions() {
    if (this.suggestions.length === 0) {
      this.suggestionsContainer.innerHTML = '';
      return;
    }

    const html = this.suggestions
      .map((suggestion, index) => {
        const icon = this.getIconForType(suggestion.type);
        const isSelected = index === this.selectedIndex ? 'selected' : '';
        return `
          <div class="suggestion-item ${isSelected}" data-index="${index}">
            <span class="suggestion-icon">${icon}</span>
            <div class="suggestion-text">
              <div class="suggestion-name">${this.escapeHtml(suggestion.name)}</div>
              <div class="suggestion-type">${suggestion.type}</div>
            </div>
          </div>
        `;
      })
      .join('');

    this.suggestionsContainer.innerHTML = html;

    // Add click handlers
    this.suggestionsContainer.querySelectorAll('.suggestion-item').forEach((item, index) => {
      item.addEventListener('click', () => {
        this.selectSuggestion(index);
      });
    });
  }

  /**
   * Get icon based on suggestion type
   */
  getIconForType(type) {
    const icons = {
      dog: '🐕',
      shelter: '🏢',
      state: '📍',
    };
    return icons[type] || '🔍';
  }

  /**
   * Handle keyboard navigation
   */
  handleKeydown(e) {
    if (!this.isOpen) {
      if (e.key === 'Enter' && this.searchInput.value.trim()) {
        e.preventDefault();
        this.navigateToSearch(this.searchInput.value.trim());
      }
      return;
    }

    switch (e.key) {
      case 'ArrowUp':
        e.preventDefault();
        this.moveSelection(-1);
        break;
      case 'ArrowDown':
        e.preventDefault();
        this.moveSelection(1);
        break;
      case 'Enter':
        e.preventDefault();
        if (this.selectedIndex >= 0) {
          this.selectSuggestion(this.selectedIndex);
        } else if (this.searchInput.value.trim()) {
          this.navigateToSearch(this.searchInput.value.trim());
        }
        break;
      case 'Escape':
        e.preventDefault();
        this.closeSuggestions();
        break;
    }
  }

  /**
   * Move selection up or down
   */
  moveSelection(direction) {
    const newIndex = this.selectedIndex + direction;

    if (newIndex >= -1 && newIndex < this.suggestions.length) {
      this.selectedIndex = newIndex;
      this.renderSuggestions();

      // Scroll selected item into view
      if (this.selectedIndex >= 0) {
        const selected = this.suggestionsContainer.querySelector(
          `[data-index="${this.selectedIndex}"]`
        );
        if (selected) {
          selected.scrollIntoView({ block: 'nearest' });
        }
      }
    }
  }

  /**
   * Select a suggestion and navigate
   */
  selectSuggestion(index) {
    const suggestion = this.suggestions[index];
    if (!suggestion) return;

    let url = '';

    switch (suggestion.type.toLowerCase()) {
      case 'dog':
        url = `/dog.html?id=${encodeURIComponent(suggestion.id)}`;
        break;
      case 'shelter':
        url = `/shelter.html?id=${encodeURIComponent(suggestion.id)}`;
        break;
      case 'state':
        url = `/state.html?code=${encodeURIComponent(suggestion.code || suggestion.id)}`;
        break;
    }

    if (url) {
      this.closeSuggestions();
      window.location.href = url;
    }
  }

  /**
   * Navigate to search results page
   */
  navigateToSearch(query) {
    this.closeSuggestions();
    window.location.href = `/search.html?q=${encodeURIComponent(query)}`;
  }

  /**
   * Open suggestions dropdown
   */
  openSuggestions() {
    this.isOpen = true;
    this.suggestionsContainer.style.display = 'block';
  }

  /**
   * Close suggestions dropdown
   */
  closeSuggestions() {
    this.isOpen = false;
    this.suggestionsContainer.style.display = 'none';
    this.suggestionsContainer.innerHTML = '';
    this.selectedIndex = -1;
    this.suggestions = [];
  }

  /**
   * Escape HTML special characters
   */
  escapeHtml(text) {
    const map = {
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      '"': '&quot;',
      "'": '&#039;',
    };
    return text.replace(/[&<>"']/g, (m) => map[m]);
  }

  /**
   * Get singleton instance
   */
  static getInstance() {
    return SearchManager.instance || new SearchManager();
  }
}

// Export to global scope
window.SearchManager = SearchManager;
