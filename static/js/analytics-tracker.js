/**
 * @file analytics-tracker.js
 * @brief Client-side analytics tracking for WaitingTheLongest.com
 *
 * PURPOSE:
 * Provides client-side analytics tracking for:
 * - Page views and navigation
 * - Dog profile views and interactions
 * - Search queries and filters
 * - User engagement (shares, favorites, clicks)
 * - Session tracking and visitor identification
 * - UTM campaign tracking
 *
 * USAGE:
 * // Initialize tracker (auto-initialized on page load)
 * const tracker = WTLAnalytics.getInstance();
 *
 * // Track page view (automatic on page load)
 * tracker.trackPageView();
 *
 * // Track dog view
 * tracker.trackDogView('dog-uuid', { name: 'Buddy' });
 *
 * // Track search
 * tracker.trackSearch('golden retriever', { state: 'TX' });
 *
 * // Track share
 * tracker.trackShare('dog', 'dog-uuid', 'twitter');
 *
 * DEPENDENCIES:
 * - None (vanilla JavaScript)
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

(function(global) {
    'use strict';

    // =========================================================================
    // CONSTANTS
    // =========================================================================

    /**
     * Event types matching server-side AnalyticsEventType enum
     */
    const EventType = {
        // Page events
        PAGE_VIEW: 'PAGE_VIEW',
        PAGE_EXIT: 'PAGE_EXIT',
        PAGE_SCROLL: 'PAGE_SCROLL',

        // Search events
        SEARCH: 'SEARCH',
        SEARCH_FILTER: 'SEARCH_FILTER',
        SEARCH_RESULT_CLICK: 'SEARCH_RESULT_CLICK',

        // Dog events
        DOG_VIEW: 'DOG_VIEW',
        DOG_FAVORITED: 'DOG_FAVORITED',
        DOG_UNFAVORITED: 'DOG_UNFAVORITED',
        DOG_SHARED: 'DOG_SHARED',
        DOG_CONTACT_CLICKED: 'DOG_CONTACT_CLICKED',
        DOG_DIRECTIONS_CLICKED: 'DOG_DIRECTIONS_CLICKED',
        DOG_GALLERY_VIEW: 'DOG_GALLERY_VIEW',

        // Shelter events
        SHELTER_VIEW: 'SHELTER_VIEW',
        SHELTER_CONTACT_CLICKED: 'SHELTER_CONTACT_CLICKED',
        SHELTER_DIRECTIONS_CLICKED: 'SHELTER_DIRECTIONS_CLICKED',

        // Foster/adoption events
        FOSTER_APPLICATION_STARTED: 'FOSTER_APPLICATION_STARTED',
        FOSTER_APPLICATION_SUBMITTED: 'FOSTER_APPLICATION_SUBMITTED',
        ADOPTION_INQUIRY_STARTED: 'ADOPTION_INQUIRY_STARTED',

        // Social events
        SOCIAL_SHARE_INITIATED: 'SOCIAL_SHARE_INITIATED',
        SOCIAL_SHARE_COMPLETED: 'SOCIAL_SHARE_COMPLETED',

        // User events
        USER_SIGNUP: 'USER_SIGNUP',
        USER_LOGIN: 'USER_LOGIN',
        USER_LOGOUT: 'USER_LOGOUT',

        // Notification events
        NOTIFICATION_PERMISSION_GRANTED: 'NOTIFICATION_PERMISSION_GRANTED',
        NOTIFICATION_PERMISSION_DENIED: 'NOTIFICATION_PERMISSION_DENIED',
        EMAIL_SUBSCRIBE: 'EMAIL_SUBSCRIBE',
        EMAIL_UNSUBSCRIBE: 'EMAIL_UNSUBSCRIBE'
    };

    /**
     * Storage keys
     */
    const StorageKeys = {
        VISITOR_ID: 'wtl_visitor_id',
        SESSION_ID: 'wtl_session_id',
        SESSION_START: 'wtl_session_start',
        LAST_ACTIVITY: 'wtl_last_activity',
        PAGE_VIEW_COUNT: 'wtl_page_views',
        CONSENT_GIVEN: 'wtl_consent'
    };

    /**
     * Configuration defaults
     */
    const DefaultConfig = {
        apiEndpoint: '/api/analytics',
        batchSize: 10,
        batchTimeout: 5000,
        sessionTimeout: 30 * 60 * 1000, // 30 minutes
        scrollDepthThresholds: [25, 50, 75, 90, 100],
        enableScrollTracking: true,
        enableExitTracking: true,
        enablePerformanceTracking: true,
        debug: false
    };

    // =========================================================================
    // UTILITY FUNCTIONS
    // =========================================================================

    /**
     * Generate a UUID v4
     * @returns {string} UUID string
     */
    function generateUUID() {
        if (typeof crypto !== 'undefined' && crypto.randomUUID) {
            return crypto.randomUUID();
        }
        // Fallback for older browsers
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }

    /**
     * Get current timestamp in ISO format
     * @returns {string} ISO timestamp
     */
    function getTimestamp() {
        return new Date().toISOString();
    }

    /**
     * Parse URL query parameters
     * @param {string} url URL to parse
     * @returns {Object} Parsed parameters
     */
    function parseQueryParams(url) {
        const params = {};
        try {
            const urlObj = new URL(url, window.location.origin);
            urlObj.searchParams.forEach((value, key) => {
                params[key] = value;
            });
        } catch (e) {
            // Fallback for invalid URLs
        }
        return params;
    }

    /**
     * Get UTM parameters from URL
     * @returns {Object} UTM parameters
     */
    function getUTMParams() {
        const params = parseQueryParams(window.location.href);
        return {
            utm_source: params.utm_source || null,
            utm_medium: params.utm_medium || null,
            utm_campaign: params.utm_campaign || null,
            utm_term: params.utm_term || null,
            utm_content: params.utm_content || null
        };
    }

    /**
     * Get device type based on screen size and user agent
     * @returns {string} Device type (desktop, mobile, tablet)
     */
    function getDeviceType() {
        const ua = navigator.userAgent.toLowerCase();
        const width = window.innerWidth;

        if (/mobile|android|iphone|ipod|blackberry|opera mini|iemobile/i.test(ua)) {
            return 'mobile';
        }
        if (/ipad|tablet|playbook|silk/i.test(ua) || (width >= 768 && width <= 1024)) {
            return 'tablet';
        }
        return 'desktop';
    }

    /**
     * Get browser name from user agent
     * @returns {string} Browser name
     */
    function getBrowser() {
        const ua = navigator.userAgent;
        if (ua.includes('Firefox')) return 'Firefox';
        if (ua.includes('SamsungBrowser')) return 'Samsung Browser';
        if (ua.includes('Opera') || ua.includes('OPR')) return 'Opera';
        if (ua.includes('Trident')) return 'IE';
        if (ua.includes('Edge')) return 'Edge';
        if (ua.includes('Edg')) return 'Edge Chromium';
        if (ua.includes('Chrome')) return 'Chrome';
        if (ua.includes('Safari')) return 'Safari';
        return 'Unknown';
    }

    /**
     * Get operating system from user agent
     * @returns {string} OS name
     */
    function getOS() {
        const ua = navigator.userAgent;
        if (ua.includes('Windows NT 10')) return 'Windows 10';
        if (ua.includes('Windows NT 6.3')) return 'Windows 8.1';
        if (ua.includes('Windows NT 6.2')) return 'Windows 8';
        if (ua.includes('Windows NT 6.1')) return 'Windows 7';
        if (ua.includes('Windows')) return 'Windows';
        if (ua.includes('Mac OS X')) return 'macOS';
        if (ua.includes('Android')) return 'Android';
        if (ua.includes('iPhone') || ua.includes('iPad')) return 'iOS';
        if (ua.includes('Linux')) return 'Linux';
        return 'Unknown';
    }

    /**
     * Get page scroll depth percentage
     * @returns {number} Scroll depth 0-100
     */
    function getScrollDepth() {
        const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
        const scrollHeight = document.documentElement.scrollHeight;
        const clientHeight = document.documentElement.clientHeight;
        const maxScroll = scrollHeight - clientHeight;
        if (maxScroll <= 0) return 100;
        return Math.round((scrollTop / maxScroll) * 100);
    }

    /**
     * Safely get localStorage item
     * @param {string} key Storage key
     * @returns {string|null} Stored value or null
     */
    function getStorage(key) {
        try {
            return localStorage.getItem(key);
        } catch (e) {
            return null;
        }
    }

    /**
     * Safely set localStorage item
     * @param {string} key Storage key
     * @param {string} value Value to store
     */
    function setStorage(key, value) {
        try {
            localStorage.setItem(key, value);
        } catch (e) {
            // Storage not available
        }
    }

    /**
     * Safely get sessionStorage item
     * @param {string} key Storage key
     * @returns {string|null} Stored value or null
     */
    function getSessionStorage(key) {
        try {
            return sessionStorage.getItem(key);
        } catch (e) {
            return null;
        }
    }

    /**
     * Safely set sessionStorage item
     * @param {string} key Storage key
     * @param {string} value Value to store
     */
    function setSessionStorage(key, value) {
        try {
            sessionStorage.setItem(key, value);
        } catch (e) {
            // Storage not available
        }
    }

    // =========================================================================
    // WTLANALYTICS CLASS
    // =========================================================================

    /**
     * WTLAnalytics
     * Main analytics tracking class for WaitingTheLongest.com
     */
    class WTLAnalytics {
        /**
         * Singleton instance
         * @type {WTLAnalytics|null}
         */
        static _instance = null;

        /**
         * Get singleton instance
         * @param {Object} config Configuration options
         * @returns {WTLAnalytics} Singleton instance
         */
        static getInstance(config = {}) {
            if (!WTLAnalytics._instance) {
                WTLAnalytics._instance = new WTLAnalytics(config);
            }
            return WTLAnalytics._instance;
        }

        /**
         * Create analytics tracker instance
         * @param {Object} config Configuration options
         */
        constructor(config = {}) {
            this.config = { ...DefaultConfig, ...config };
            this.eventQueue = [];
            this.batchTimer = null;
            this.initialized = false;
            this.scrollDepthsReached = new Set();
            this.pageStartTime = Date.now();
            this.userId = null;

            // Initialize on construction
            this._initialize();
        }

        /**
         * Initialize the tracker
         * @private
         */
        _initialize() {
            if (this.initialized) return;

            // Initialize visitor and session
            this._initializeVisitor();
            this._initializeSession();

            // Setup event listeners
            this._setupEventListeners();

            // Track initial page view
            this.trackPageView();

            // Track performance metrics
            if (this.config.enablePerformanceTracking) {
                this._trackPerformance();
            }

            this.initialized = true;
            this._log('Analytics tracker initialized');
        }

        /**
         * Initialize or retrieve visitor ID
         * @private
         */
        _initializeVisitor() {
            let visitorId = getStorage(StorageKeys.VISITOR_ID);
            if (!visitorId) {
                visitorId = generateUUID();
                setStorage(StorageKeys.VISITOR_ID, visitorId);
            }
            this.visitorId = visitorId;
        }

        /**
         * Initialize or retrieve session
         * @private
         */
        _initializeSession() {
            const lastActivity = parseInt(getSessionStorage(StorageKeys.LAST_ACTIVITY) || '0', 10);
            const now = Date.now();

            // Check if session expired
            if (lastActivity && (now - lastActivity) > this.config.sessionTimeout) {
                // Session expired, create new one
                this._createNewSession();
            } else {
                // Try to restore existing session
                let sessionId = getSessionStorage(StorageKeys.SESSION_ID);
                if (!sessionId) {
                    this._createNewSession();
                } else {
                    this.sessionId = sessionId;
                    this.sessionStart = parseInt(getSessionStorage(StorageKeys.SESSION_START) || String(now), 10);
                }
            }

            // Update last activity
            setSessionStorage(StorageKeys.LAST_ACTIVITY, String(now));
        }

        /**
         * Create a new session
         * @private
         */
        _createNewSession() {
            this.sessionId = generateUUID();
            this.sessionStart = Date.now();
            setSessionStorage(StorageKeys.SESSION_ID, this.sessionId);
            setSessionStorage(StorageKeys.SESSION_START, String(this.sessionStart));
            setSessionStorage(StorageKeys.PAGE_VIEW_COUNT, '0');
        }

        /**
         * Setup DOM event listeners
         * @private
         */
        _setupEventListeners() {
            // Track scroll depth
            if (this.config.enableScrollTracking) {
                let scrollTimeout = null;
                window.addEventListener('scroll', () => {
                    if (scrollTimeout) clearTimeout(scrollTimeout);
                    scrollTimeout = setTimeout(() => {
                        this._trackScrollDepth();
                    }, 150);
                }, { passive: true });
            }

            // Track page exit
            if (this.config.enableExitTracking) {
                window.addEventListener('beforeunload', () => {
                    this._trackPageExit();
                });

                // Also track visibility change
                document.addEventListener('visibilitychange', () => {
                    if (document.visibilityState === 'hidden') {
                        this._flushQueue(true);
                    }
                });
            }

            // Track activity for session management
            ['click', 'scroll', 'keypress'].forEach(event => {
                window.addEventListener(event, () => {
                    setSessionStorage(StorageKeys.LAST_ACTIVITY, String(Date.now()));
                }, { passive: true, once: false });
            });

            // Auto-track clicks on elements with data-analytics attributes
            document.addEventListener('click', (e) => {
                this._handleAutoTrack(e);
            });
        }

        /**
         * Handle auto-tracking from data attributes
         * @param {Event} e Click event
         * @private
         */
        _handleAutoTrack(e) {
            const target = e.target.closest('[data-analytics-event]');
            if (!target) return;

            const eventType = target.getAttribute('data-analytics-event');
            const entityType = target.getAttribute('data-analytics-entity-type');
            const entityId = target.getAttribute('data-analytics-entity-id');
            const dataStr = target.getAttribute('data-analytics-data');

            let data = {};
            if (dataStr) {
                try {
                    data = JSON.parse(dataStr);
                } catch (e) {
                    // Invalid JSON
                }
            }

            this.trackEvent(eventType, entityType, entityId, data);
        }

        /**
         * Track scroll depth
         * @private
         */
        _trackScrollDepth() {
            const depth = getScrollDepth();
            for (const threshold of this.config.scrollDepthThresholds) {
                if (depth >= threshold && !this.scrollDepthsReached.has(threshold)) {
                    this.scrollDepthsReached.add(threshold);
                    this.trackEvent(EventType.PAGE_SCROLL, 'page', null, {
                        scroll_depth: threshold,
                        page_url: window.location.href
                    });
                }
            }
        }

        /**
         * Track page exit
         * @private
         */
        _trackPageExit() {
            const timeOnPage = Math.round((Date.now() - this.pageStartTime) / 1000);
            const maxScrollDepth = Math.max(...this.scrollDepthsReached, 0);

            this.trackEvent(EventType.PAGE_EXIT, 'page', null, {
                time_on_page_seconds: timeOnPage,
                max_scroll_depth: maxScrollDepth,
                page_url: window.location.href
            });

            // Flush immediately on exit
            this._flushQueue(true);
        }

        /**
         * Track page performance metrics
         * @private
         */
        _trackPerformance() {
            // Wait for page to fully load
            if (document.readyState === 'complete') {
                this._capturePerformance();
            } else {
                window.addEventListener('load', () => {
                    // Give a small delay for all resources
                    setTimeout(() => this._capturePerformance(), 100);
                });
            }
        }

        /**
         * Capture performance metrics
         * @private
         */
        _capturePerformance() {
            if (!window.performance || !window.performance.timing) return;

            const timing = window.performance.timing;
            const data = {
                dns_time: timing.domainLookupEnd - timing.domainLookupStart,
                connect_time: timing.connectEnd - timing.connectStart,
                ttfb: timing.responseStart - timing.requestStart,
                dom_load_time: timing.domContentLoadedEventEnd - timing.navigationStart,
                page_load_time: timing.loadEventEnd - timing.navigationStart
            };

            // Only track if we have valid data
            if (data.page_load_time > 0) {
                this.trackEvent('PERFORMANCE', 'page', null, data);
            }
        }

        // =====================================================================
        // PUBLIC TRACKING METHODS
        // =====================================================================

        /**
         * Set the user ID for authenticated users
         * @param {string|null} userId User ID or null to clear
         */
        setUserId(userId) {
            this.userId = userId;
            this._log('User ID set:', userId);
        }

        /**
         * Track a generic event
         * @param {string} eventType Event type
         * @param {string} entityType Entity type (dog, shelter, page, etc.)
         * @param {string|null} entityId Entity ID
         * @param {Object} data Additional event data
         */
        trackEvent(eventType, entityType = null, entityId = null, data = {}) {
            const event = this._buildEvent(eventType, entityType, entityId, data);
            this._queueEvent(event);
        }

        /**
         * Track page view
         * @param {Object} data Additional data
         */
        trackPageView(data = {}) {
            // Increment page view count
            const count = parseInt(getSessionStorage(StorageKeys.PAGE_VIEW_COUNT) || '0', 10) + 1;
            setSessionStorage(StorageKeys.PAGE_VIEW_COUNT, String(count));

            const utmParams = getUTMParams();
            const eventData = {
                page_title: document.title,
                page_path: window.location.pathname,
                page_url: window.location.href,
                referrer: document.referrer || null,
                page_view_number: count,
                ...utmParams,
                ...data
            };

            this.trackEvent(EventType.PAGE_VIEW, 'page', null, eventData);
        }

        /**
         * Track dog profile view
         * @param {string} dogId Dog ID
         * @param {Object} data Additional data (name, shelter_id, urgency, etc.)
         */
        trackDogView(dogId, data = {}) {
            this.trackEvent(EventType.DOG_VIEW, 'dog', dogId, data);
        }

        /**
         * Track shelter page view
         * @param {string} shelterId Shelter ID
         * @param {Object} data Additional data (name, state, etc.)
         */
        trackShelterView(shelterId, data = {}) {
            this.trackEvent(EventType.SHELTER_VIEW, 'shelter', shelterId, data);
        }

        /**
         * Track search
         * @param {string} query Search query
         * @param {Object} filters Applied filters
         * @param {number} resultCount Number of results
         */
        trackSearch(query, filters = {}, resultCount = null) {
            this.trackEvent(EventType.SEARCH, 'search', null, {
                query: query,
                filters: filters,
                result_count: resultCount
            });
        }

        /**
         * Track search filter change
         * @param {string} filterName Filter name
         * @param {*} filterValue Filter value
         */
        trackSearchFilter(filterName, filterValue) {
            this.trackEvent(EventType.SEARCH_FILTER, 'search', null, {
                filter_name: filterName,
                filter_value: filterValue
            });
        }

        /**
         * Track search result click
         * @param {string} dogId Dog ID that was clicked
         * @param {number} position Position in results
         * @param {string} query Search query
         */
        trackSearchResultClick(dogId, position, query) {
            this.trackEvent(EventType.SEARCH_RESULT_CLICK, 'dog', dogId, {
                position: position,
                query: query
            });
        }

        /**
         * Track favorite added
         * @param {string} dogId Dog ID
         * @param {Object} data Additional data
         */
        trackFavorite(dogId, data = {}) {
            this.trackEvent(EventType.DOG_FAVORITED, 'dog', dogId, data);
        }

        /**
         * Track favorite removed
         * @param {string} dogId Dog ID
         * @param {Object} data Additional data
         */
        trackUnfavorite(dogId, data = {}) {
            this.trackEvent(EventType.DOG_UNFAVORITED, 'dog', dogId, data);
        }

        /**
         * Track share action
         * @param {string} entityType Entity type (dog, shelter, page)
         * @param {string} entityId Entity ID
         * @param {string} platform Share platform (twitter, facebook, email, etc.)
         * @param {Object} data Additional data
         */
        trackShare(entityType, entityId, platform, data = {}) {
            this.trackEvent(EventType.DOG_SHARED, entityType, entityId, {
                share_platform: platform,
                ...data
            });
        }

        /**
         * Track social share initiated
         * @param {string} entityType Entity type
         * @param {string} entityId Entity ID
         * @param {string} platform Social platform
         */
        trackSocialShareInitiated(entityType, entityId, platform) {
            this.trackEvent(EventType.SOCIAL_SHARE_INITIATED, entityType, entityId, {
                platform: platform
            });
        }

        /**
         * Track social share completed
         * @param {string} entityType Entity type
         * @param {string} entityId Entity ID
         * @param {string} platform Social platform
         */
        trackSocialShareCompleted(entityType, entityId, platform) {
            this.trackEvent(EventType.SOCIAL_SHARE_COMPLETED, entityType, entityId, {
                platform: platform
            });
        }

        /**
         * Track contact button click
         * @param {string} entityType Entity type (dog, shelter)
         * @param {string} entityId Entity ID
         * @param {string} contactType Contact type (phone, email, form)
         */
        trackContactClick(entityType, entityId, contactType) {
            const eventType = entityType === 'shelter'
                ? EventType.SHELTER_CONTACT_CLICKED
                : EventType.DOG_CONTACT_CLICKED;
            this.trackEvent(eventType, entityType, entityId, {
                contact_type: contactType
            });
        }

        /**
         * Track directions button click
         * @param {string} entityType Entity type (dog, shelter)
         * @param {string} entityId Entity ID
         */
        trackDirectionsClick(entityType, entityId) {
            const eventType = entityType === 'shelter'
                ? EventType.SHELTER_DIRECTIONS_CLICKED
                : EventType.DOG_DIRECTIONS_CLICKED;
            this.trackEvent(eventType, entityType, entityId, {});
        }

        /**
         * Track gallery view
         * @param {string} dogId Dog ID
         * @param {number} imageIndex Image index viewed
         */
        trackGalleryView(dogId, imageIndex) {
            this.trackEvent(EventType.DOG_GALLERY_VIEW, 'dog', dogId, {
                image_index: imageIndex
            });
        }

        /**
         * Track foster application started
         * @param {string} dogId Dog ID
         * @param {Object} data Additional data
         */
        trackFosterApplicationStarted(dogId, data = {}) {
            this.trackEvent(EventType.FOSTER_APPLICATION_STARTED, 'dog', dogId, data);
        }

        /**
         * Track foster application submitted
         * @param {string} dogId Dog ID
         * @param {string} applicationId Application ID
         * @param {Object} data Additional data
         */
        trackFosterApplicationSubmitted(dogId, applicationId, data = {}) {
            this.trackEvent(EventType.FOSTER_APPLICATION_SUBMITTED, 'dog', dogId, {
                application_id: applicationId,
                ...data
            });
        }

        /**
         * Track adoption inquiry started
         * @param {string} dogId Dog ID
         * @param {Object} data Additional data
         */
        trackAdoptionInquiryStarted(dogId, data = {}) {
            this.trackEvent(EventType.ADOPTION_INQUIRY_STARTED, 'dog', dogId, data);
        }

        /**
         * Track user signup
         * @param {string} userId User ID
         * @param {string} method Signup method (email, google, facebook)
         */
        trackSignup(userId, method) {
            this.setUserId(userId);
            this.trackEvent(EventType.USER_SIGNUP, 'user', userId, {
                signup_method: method
            });
        }

        /**
         * Track user login
         * @param {string} userId User ID
         * @param {string} method Login method
         */
        trackLogin(userId, method) {
            this.setUserId(userId);
            this.trackEvent(EventType.USER_LOGIN, 'user', userId, {
                login_method: method
            });
        }

        /**
         * Track user logout
         */
        trackLogout() {
            const userId = this.userId;
            this.trackEvent(EventType.USER_LOGOUT, 'user', userId, {});
            this.setUserId(null);
        }

        /**
         * Track notification permission
         * @param {boolean} granted Whether permission was granted
         */
        trackNotificationPermission(granted) {
            const eventType = granted
                ? EventType.NOTIFICATION_PERMISSION_GRANTED
                : EventType.NOTIFICATION_PERMISSION_DENIED;
            this.trackEvent(eventType, 'user', this.userId, {});
        }

        /**
         * Track email subscription
         * @param {string} email Email address (hashed for privacy)
         * @param {Object} preferences Subscription preferences
         */
        trackEmailSubscribe(email, preferences = {}) {
            this.trackEvent(EventType.EMAIL_SUBSCRIBE, 'user', this.userId, {
                preferences: preferences
            });
        }

        /**
         * Track email unsubscription
         * @param {string} reason Unsubscribe reason
         */
        trackEmailUnsubscribe(reason = null) {
            this.trackEvent(EventType.EMAIL_UNSUBSCRIBE, 'user', this.userId, {
                reason: reason
            });
        }

        // =====================================================================
        // INTERNAL METHODS
        // =====================================================================

        /**
         * Build an analytics event object
         * @param {string} eventType Event type
         * @param {string} entityType Entity type
         * @param {string|null} entityId Entity ID
         * @param {Object} data Additional data
         * @returns {Object} Event object
         * @private
         */
        _buildEvent(eventType, entityType, entityId, data) {
            return {
                event_type: eventType,
                entity_type: entityType,
                entity_id: entityId,
                user_id: this.userId,
                session_id: this.sessionId,
                visitor_id: this.visitorId,
                source: 'web',
                referrer: document.referrer || null,
                page_url: window.location.href,
                device_type: getDeviceType(),
                browser: getBrowser(),
                os: getOS(),
                data: data,
                timestamp: getTimestamp()
            };
        }

        /**
         * Queue an event for sending
         * @param {Object} event Event object
         * @private
         */
        _queueEvent(event) {
            this.eventQueue.push(event);
            this._log('Event queued:', event.event_type, event);

            // Check if we should flush
            if (this.eventQueue.length >= this.config.batchSize) {
                this._flushQueue();
            } else if (!this.batchTimer) {
                this.batchTimer = setTimeout(() => {
                    this._flushQueue();
                }, this.config.batchTimeout);
            }
        }

        /**
         * Flush event queue to server
         * @param {boolean} sync Use synchronous request (for page unload)
         * @private
         */
        _flushQueue(sync = false) {
            if (this.batchTimer) {
                clearTimeout(this.batchTimer);
                this.batchTimer = null;
            }

            if (this.eventQueue.length === 0) return;

            const events = this.eventQueue.splice(0, this.eventQueue.length);
            this._log('Flushing', events.length, 'events');

            const url = this.config.apiEndpoint + '/batch';
            const body = JSON.stringify({ events: events });

            if (sync && navigator.sendBeacon) {
                // Use sendBeacon for page unload (doesn't block)
                navigator.sendBeacon(url, body);
            } else {
                // Use fetch for normal requests
                fetch(url, {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: body,
                    keepalive: true
                }).then(response => {
                    if (!response.ok) {
                        this._log('Failed to send events:', response.status);
                        // Re-queue failed events
                        this.eventQueue.unshift(...events);
                    }
                }).catch(error => {
                    this._log('Error sending events:', error);
                    // Re-queue failed events
                    this.eventQueue.unshift(...events);
                });
            }
        }

        /**
         * Log debug message
         * @param {...*} args Arguments to log
         * @private
         */
        _log(...args) {
            if (this.config.debug) {
                console.log('[WTLAnalytics]', ...args);
            }
        }
    }

    // =========================================================================
    // DATA ATTRIBUTE AUTO-TRACKING HELPER
    // =========================================================================

    /**
     * Helper function to add analytics tracking to an element
     * @param {HTMLElement} element DOM element
     * @param {string} eventType Event type
     * @param {string} entityType Entity type
     * @param {string} entityId Entity ID
     * @param {Object} data Additional data
     */
    function addAnalyticsTracking(element, eventType, entityType, entityId, data = {}) {
        element.setAttribute('data-analytics-event', eventType);
        if (entityType) element.setAttribute('data-analytics-entity-type', entityType);
        if (entityId) element.setAttribute('data-analytics-entity-id', entityId);
        if (Object.keys(data).length > 0) {
            element.setAttribute('data-analytics-data', JSON.stringify(data));
        }
    }

    // =========================================================================
    // EXPORTS
    // =========================================================================

    // Export to global scope
    global.WTLAnalytics = WTLAnalytics;
    global.WTLAnalyticsEventType = EventType;
    global.addAnalyticsTracking = addAnalyticsTracking;

    // Auto-initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => {
            WTLAnalytics.getInstance();
        });
    } else {
        // DOM already loaded
        WTLAnalytics.getInstance();
    }

})(typeof window !== 'undefined' ? window : this);
