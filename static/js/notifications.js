/**
 * @file notifications.js
 * @brief Push notification client for WaitingTheLongest.com
 *
 * PURPOSE:
 * Provides client-side push notification functionality including:
 * - Firebase Cloud Messaging (FCM) integration
 * - Service worker registration
 * - Notification permission handling
 * - Token management
 * - Notification preferences UI
 * - In-app notification display
 *
 * USAGE:
 * // Initialize notification client
 * const notifications = new NotificationClient({
 *     firebaseConfig: { ... },
 *     vapidKey: 'your-vapid-key'
 * });
 *
 * // Request permission and register
 * await notifications.requestPermission();
 *
 * // Listen for notifications
 * notifications.onNotification((notification) => {
 *     console.log('Received:', notification);
 * });
 *
 * DEPENDENCIES:
 * - Firebase Cloud Messaging SDK
 * - Service Worker API
 * - Notification API
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

(function(global) {
    'use strict';

    /**
     * Notification priority levels
     */
    const NotificationPriority = {
        CRITICAL: 1,      // Critical alerts - always show
        HIGH: 2,          // High urgency
        MEDIUM: 5,        // Matches, updates
        LOW: 8            // Tips, stories
    };

    /**
     * Notification types matching server-side enum
     */
    const NotificationType = {
        CRITICAL_ALERT: 'critical_alert',
        HIGH_URGENCY: 'high_urgency',
        FOSTER_NEEDED_URGENT: 'foster_needed_urgent',
        PERFECT_MATCH: 'perfect_match',
        GOOD_MATCH: 'good_match',
        DOG_UPDATE: 'dog_update',
        FOSTER_FOLLOWUP: 'foster_followup',
        SUCCESS_STORY: 'success_story',
        NEW_BLOG_POST: 'new_blog_post',
        TIP_OF_THE_DAY: 'tip_of_the_day',
        SYSTEM_ALERT: 'system_alert'
    };

    /**
     * Permission states
     */
    const PermissionState = {
        GRANTED: 'granted',
        DENIED: 'denied',
        DEFAULT: 'default',
        UNSUPPORTED: 'unsupported'
    };

    /**
     * NotificationClient
     * Main client for push notification functionality
     */
    class NotificationClient {
        /**
         * Create a new notification client
         * @param {Object} options Configuration options
         * @param {Object} options.firebaseConfig Firebase configuration
         * @param {string} options.vapidKey VAPID key for web push
         * @param {string} options.apiBaseUrl API base URL (default: '')
         * @param {string} options.serviceWorkerPath Service worker path (default: '/sw-notifications.js')
         */
        constructor(options = {}) {
            this.options = {
                firebaseConfig: options.firebaseConfig || null,
                vapidKey: options.vapidKey || null,
                apiBaseUrl: options.apiBaseUrl || '',
                serviceWorkerPath: options.serviceWorkerPath || '/sw-notifications.js'
            };

            // State
            this.token = null;
            this.registration = null;
            this.messaging = null;
            this.isInitialized = false;
            this.permissionState = PermissionState.DEFAULT;

            // Callbacks
            this.callbacks = {
                onNotification: [],
                onTokenRefresh: [],
                onPermissionChange: [],
                onError: []
            };

            // Initialize permission state
            this._updatePermissionState();

            // Bind methods
            this._handleForegroundMessage = this._handleForegroundMessage.bind(this);
            this._handleTokenRefresh = this._handleTokenRefresh.bind(this);
        }

        /**
         * Initialize the notification client
         * @returns {Promise<boolean>} True if initialized successfully
         */
        async initialize() {
            if (this.isInitialized) {
                return true;
            }

            // Check browser support
            if (!this._isSupported()) {
                console.warn('[Notifications] Push notifications not supported');
                this.permissionState = PermissionState.UNSUPPORTED;
                return false;
            }

            try {
                // Register service worker
                await this._registerServiceWorker();

                // Initialize Firebase if config provided
                if (this.options.firebaseConfig) {
                    await this._initializeFirebase();
                }

                this.isInitialized = true;
                console.log('[Notifications] Client initialized');
                return true;
            } catch (error) {
                console.error('[Notifications] Initialization failed:', error);
                this._emit('onError', { message: 'Initialization failed', error });
                return false;
            }
        }

        /**
         * Request notification permission
         * @returns {Promise<string>} Permission state
         */
        async requestPermission() {
            if (!this._isSupported()) {
                return PermissionState.UNSUPPORTED;
            }

            try {
                const permission = await Notification.requestPermission();
                this.permissionState = permission;
                this._emit('onPermissionChange', { permission });

                if (permission === PermissionState.GRANTED) {
                    // Get and register token
                    await this._getAndRegisterToken();
                }

                return permission;
            } catch (error) {
                console.error('[Notifications] Permission request failed:', error);
                return PermissionState.DENIED;
            }
        }

        /**
         * Get current permission state
         * @returns {string} Permission state
         */
        getPermissionState() {
            this._updatePermissionState();
            return this.permissionState;
        }

        /**
         * Check if notifications are enabled
         * @returns {boolean} True if enabled
         */
        isEnabled() {
            return this.permissionState === PermissionState.GRANTED && this.token !== null;
        }

        /**
         * Get current FCM token
         * @returns {string|null} FCM token
         */
        getToken() {
            return this.token;
        }

        /**
         * Register event callback
         * @param {string} event Event name
         * @param {Function} callback Callback function
         */
        on(event, callback) {
            const eventName = 'on' + event.charAt(0).toUpperCase() + event.slice(1);
            if (this.callbacks[eventName]) {
                this.callbacks[eventName].push(callback);
            }
        }

        /**
         * Remove event callback
         * @param {string} event Event name
         * @param {Function} callback Callback function
         */
        off(event, callback) {
            const eventName = 'on' + event.charAt(0).toUpperCase() + event.slice(1);
            if (this.callbacks[eventName]) {
                const index = this.callbacks[eventName].indexOf(callback);
                if (index > -1) {
                    this.callbacks[eventName].splice(index, 1);
                }
            }
        }

        /**
         * Unregister device and token
         * @returns {Promise<boolean>} True if successful
         */
        async unregister() {
            if (!this.token) {
                return true;
            }

            try {
                // Unregister from server
                await this._unregisterToken(this.token);

                // Delete token from Firebase
                if (this.messaging) {
                    await this.messaging.deleteToken();
                }

                this.token = null;
                console.log('[Notifications] Device unregistered');
                return true;
            } catch (error) {
                console.error('[Notifications] Unregister failed:', error);
                return false;
            }
        }

        /**
         * Show a local notification (not from push)
         * @param {Object} notification Notification data
         */
        showLocalNotification(notification) {
            if (this.permissionState !== PermissionState.GRANTED) {
                console.warn('[Notifications] Permission not granted');
                return;
            }

            const options = {
                body: notification.body,
                icon: notification.icon || '/static/images/logo-icon.png',
                badge: '/static/images/badge.png',
                tag: notification.tag || 'wtl-notification',
                data: notification.data || {},
                requireInteraction: notification.priority <= NotificationPriority.HIGH,
                actions: notification.actions || []
            };

            if (notification.image) {
                options.image = notification.image;
            }

            if (this.registration) {
                this.registration.showNotification(notification.title, options);
            } else {
                new Notification(notification.title, options);
            }
        }

        // ====================================================================
        // PREFERENCES API
        // ====================================================================

        /**
         * Get notification preferences from server
         * @returns {Promise<Object>} Preferences object
         */
        async getPreferences() {
            const response = await this._apiRequest('GET', '/api/notifications/preferences');
            return response.data;
        }

        /**
         * Update notification preferences
         * @param {Object} preferences New preferences
         * @returns {Promise<Object>} Updated preferences
         */
        async updatePreferences(preferences) {
            const response = await this._apiRequest('PUT', '/api/notifications/preferences', preferences);
            return response.data;
        }

        /**
         * Get notification history
         * @param {Object} options Query options
         * @returns {Promise<Array>} Notifications array
         */
        async getNotifications(options = {}) {
            const params = new URLSearchParams();
            if (options.page) params.set('page', options.page);
            if (options.perPage) params.set('per_page', options.perPage);
            if (options.type) params.set('type', options.type);
            if (options.unreadOnly) params.set('unread_only', 'true');

            const url = '/api/notifications' + (params.toString() ? '?' + params.toString() : '');
            const response = await this._apiRequest('GET', url);
            return response;
        }

        /**
         * Mark notification as read
         * @param {string} notificationId Notification ID
         * @returns {Promise<boolean>} Success
         */
        async markAsRead(notificationId) {
            await this._apiRequest('PUT', `/api/notifications/${notificationId}/read`);
            return true;
        }

        /**
         * Mark all notifications as read
         * @returns {Promise<Object>} Result
         */
        async markAllAsRead() {
            const response = await this._apiRequest('PUT', '/api/notifications/read-all');
            return response.data;
        }

        /**
         * Get unread count
         * @returns {Promise<number>} Unread count
         */
        async getUnreadCount() {
            const response = await this._apiRequest('GET', '/api/notifications/unread-count');
            return response.data.unread_count;
        }

        /**
         * Delete a notification
         * @param {string} notificationId Notification ID
         * @returns {Promise<boolean>} Success
         */
        async deleteNotification(notificationId) {
            await this._apiRequest('DELETE', `/api/notifications/${notificationId}`);
            return true;
        }

        // ====================================================================
        // PRIVATE METHODS
        // ====================================================================

        _isSupported() {
            return 'serviceWorker' in navigator &&
                   'PushManager' in window &&
                   'Notification' in window;
        }

        _updatePermissionState() {
            if (!this._isSupported()) {
                this.permissionState = PermissionState.UNSUPPORTED;
            } else {
                this.permissionState = Notification.permission;
            }
        }

        async _registerServiceWorker() {
            try {
                this.registration = await navigator.serviceWorker.register(
                    this.options.serviceWorkerPath,
                    { scope: '/' }
                );

                console.log('[Notifications] Service worker registered');

                // Listen for messages from service worker
                navigator.serviceWorker.addEventListener('message', (event) => {
                    if (event.data && event.data.type === 'NOTIFICATION_CLICKED') {
                        this._handleNotificationClick(event.data.notification);
                    }
                });

                return this.registration;
            } catch (error) {
                console.error('[Notifications] Service worker registration failed:', error);
                throw error;
            }
        }

        async _initializeFirebase() {
            // Check if Firebase is loaded
            if (typeof firebase === 'undefined') {
                console.warn('[Notifications] Firebase SDK not loaded');
                return;
            }

            try {
                // Initialize Firebase app if not already done
                if (!firebase.apps.length) {
                    firebase.initializeApp(this.options.firebaseConfig);
                }

                this.messaging = firebase.messaging();

                // Use service worker for messaging
                if (this.registration) {
                    this.messaging.useServiceWorker(this.registration);
                }

                // Listen for foreground messages
                this.messaging.onMessage(this._handleForegroundMessage);

                console.log('[Notifications] Firebase initialized');
            } catch (error) {
                console.error('[Notifications] Firebase initialization failed:', error);
                throw error;
            }
        }

        async _getAndRegisterToken() {
            try {
                // Get token from Firebase
                if (this.messaging && this.options.vapidKey) {
                    this.token = await this.messaging.getToken({
                        vapidKey: this.options.vapidKey,
                        serviceWorkerRegistration: this.registration
                    });
                } else {
                    // Fallback: use native push subscription
                    const subscription = await this.registration.pushManager.subscribe({
                        userVisibleOnly: true,
                        applicationServerKey: this._urlBase64ToUint8Array(this.options.vapidKey)
                    });
                    this.token = JSON.stringify(subscription);
                }

                // Register token with server
                await this._registerToken(this.token);

                console.log('[Notifications] Token registered');
                return this.token;
            } catch (error) {
                console.error('[Notifications] Token registration failed:', error);
                throw error;
            }
        }

        async _registerToken(token) {
            await this._apiRequest('POST', '/api/notifications/register-device', {
                token: token,
                device_type: 'web',
                device_name: this._getDeviceName()
            });
        }

        async _unregisterToken(token) {
            await this._apiRequest('DELETE', '/api/notifications/unregister-device', {
                token: token
            });
        }

        _handleForegroundMessage(payload) {
            console.log('[Notifications] Foreground message:', payload);

            // Show notification
            const notification = {
                title: payload.notification?.title || payload.data?.title || 'Notification',
                body: payload.notification?.body || payload.data?.body || '',
                icon: payload.notification?.icon || payload.data?.icon,
                image: payload.notification?.image || payload.data?.image,
                data: payload.data || {},
                priority: parseInt(payload.data?.priority) || NotificationPriority.MEDIUM
            };

            // Emit to listeners
            this._emit('onNotification', notification);

            // Show local notification
            this.showLocalNotification(notification);
        }

        _handleTokenRefresh() {
            console.log('[Notifications] Token refreshed');
            this._getAndRegisterToken().then(() => {
                this._emit('onTokenRefresh', { token: this.token });
            }).catch(error => {
                console.error('[Notifications] Token refresh failed:', error);
            });
        }

        _handleNotificationClick(notification) {
            console.log('[Notifications] Clicked:', notification);

            // Track click
            if (notification.data?.notification_id) {
                this._apiRequest('POST', `/api/notifications/${notification.data.notification_id}/clicked`, {
                    action: notification.action || 'default'
                }).catch(console.error);
            }

            // Handle navigation
            if (notification.data?.action_url) {
                window.location.href = notification.data.action_url;
            }
        }

        async _apiRequest(method, path, body = null) {
            const url = this.options.apiBaseUrl + path;
            const options = {
                method,
                headers: {
                    'Content-Type': 'application/json'
                },
                credentials: 'include'
            };

            if (body && (method === 'POST' || method === 'PUT' || method === 'DELETE')) {
                options.body = JSON.stringify(body);
            }

            const response = await fetch(url, options);

            if (!response.ok) {
                const error = await response.json().catch(() => ({ message: 'Request failed' }));
                throw new Error(error.message || `HTTP ${response.status}`);
            }

            if (response.status === 204) {
                return { success: true };
            }

            return response.json();
        }

        _emit(eventName, data) {
            if (this.callbacks[eventName]) {
                this.callbacks[eventName].forEach(callback => {
                    try {
                        callback(data);
                    } catch (error) {
                        console.error('[Notifications] Callback error:', error);
                    }
                });
            }
        }

        _getDeviceName() {
            const ua = navigator.userAgent;
            if (/Chrome/.test(ua)) return 'Chrome Browser';
            if (/Firefox/.test(ua)) return 'Firefox Browser';
            if (/Safari/.test(ua)) return 'Safari Browser';
            if (/Edge/.test(ua)) return 'Edge Browser';
            return 'Web Browser';
        }

        _urlBase64ToUint8Array(base64String) {
            const padding = '='.repeat((4 - base64String.length % 4) % 4);
            const base64 = (base64String + padding)
                .replace(/-/g, '+')
                .replace(/_/g, '/');

            const rawData = window.atob(base64);
            const outputArray = new Uint8Array(rawData.length);

            for (let i = 0; i < rawData.length; ++i) {
                outputArray[i] = rawData.charCodeAt(i);
            }
            return outputArray;
        }
    }

    /**
     * NotificationBadge
     * UI component for notification badge/counter
     */
    class NotificationBadge {
        constructor(element, client) {
            this.element = element;
            this.client = client;
            this.count = 0;

            // Auto-update periodically
            this._updateInterval = setInterval(() => this.update(), 60000);
            this.update();
        }

        async update() {
            try {
                const count = await this.client.getUnreadCount();
                this.setCount(count);
            } catch (error) {
                console.error('[NotificationBadge] Update failed:', error);
            }
        }

        setCount(count) {
            this.count = count;
            if (this.element) {
                this.element.textContent = count > 99 ? '99+' : count.toString();
                this.element.style.display = count > 0 ? 'inline-block' : 'none';
            }
        }

        destroy() {
            clearInterval(this._updateInterval);
        }
    }

    /**
     * NotificationDropdown
     * UI component for notification dropdown list
     */
    class NotificationDropdown {
        constructor(container, client, options = {}) {
            this.container = container;
            this.client = client;
            this.options = {
                maxItems: options.maxItems || 10,
                showEmpty: options.showEmpty !== false
            };

            this.notifications = [];
            this.isOpen = false;

            this._setupEventListeners();
        }

        async load() {
            try {
                const response = await this.client.getNotifications({
                    perPage: this.options.maxItems
                });
                this.notifications = response.data;
                this._render();
            } catch (error) {
                console.error('[NotificationDropdown] Load failed:', error);
            }
        }

        toggle() {
            this.isOpen = !this.isOpen;
            this.container.classList.toggle('open', this.isOpen);
            if (this.isOpen) {
                this.load();
            }
        }

        close() {
            this.isOpen = false;
            this.container.classList.remove('open');
        }

        _render() {
            const list = this.container.querySelector('.notification-list') || this.container;
            list.innerHTML = '';

            if (this.notifications.length === 0) {
                if (this.options.showEmpty) {
                    list.innerHTML = '<div class="notification-empty">No notifications</div>';
                }
                return;
            }

            this.notifications.forEach(notification => {
                const item = this._createNotificationItem(notification);
                list.appendChild(item);
            });
        }

        _createNotificationItem(notification) {
            const item = document.createElement('div');
            item.className = 'notification-item';
            if (!notification.read_at) {
                item.classList.add('unread');
            }

            const icon = this._getNotificationIcon(notification.type);
            const timeAgo = this._formatTimeAgo(notification.created_at);

            item.innerHTML = `
                <div class="notification-icon">${icon}</div>
                <div class="notification-content">
                    <div class="notification-title">${this._escapeHtml(notification.title)}</div>
                    <div class="notification-body">${this._escapeHtml(notification.body)}</div>
                    <div class="notification-time">${timeAgo}</div>
                </div>
            `;

            item.addEventListener('click', () => {
                this._handleItemClick(notification);
            });

            return item;
        }

        _handleItemClick(notification) {
            // Mark as read
            if (!notification.read_at) {
                this.client.markAsRead(notification.id).catch(console.error);
            }

            // Navigate if action URL
            if (notification.action_url) {
                window.location.href = notification.action_url;
            }

            this.close();
        }

        _getNotificationIcon(type) {
            const icons = {
                critical_alert: '<span class="icon-critical">!</span>',
                high_urgency: '<span class="icon-urgent">!</span>',
                foster_needed_urgent: '<span class="icon-foster">F</span>',
                perfect_match: '<span class="icon-match">M</span>',
                good_match: '<span class="icon-match">M</span>',
                dog_update: '<span class="icon-update">U</span>',
                success_story: '<span class="icon-success">S</span>',
                tip_of_the_day: '<span class="icon-tip">T</span>',
                default: '<span class="icon-default">N</span>'
            };
            return icons[type] || icons.default;
        }

        _formatTimeAgo(dateString) {
            const date = new Date(dateString);
            const now = new Date();
            const seconds = Math.floor((now - date) / 1000);

            if (seconds < 60) return 'Just now';
            if (seconds < 3600) return Math.floor(seconds / 60) + 'm ago';
            if (seconds < 86400) return Math.floor(seconds / 3600) + 'h ago';
            if (seconds < 604800) return Math.floor(seconds / 86400) + 'd ago';
            return date.toLocaleDateString();
        }

        _escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        _setupEventListeners() {
            // Close when clicking outside
            document.addEventListener('click', (e) => {
                if (!this.container.contains(e.target)) {
                    this.close();
                }
            });
        }
    }

    // Export to global scope
    global.NotificationClient = NotificationClient;
    global.NotificationBadge = NotificationBadge;
    global.NotificationDropdown = NotificationDropdown;
    global.NotificationType = NotificationType;
    global.NotificationPriority = NotificationPriority;
    global.PermissionState = PermissionState;

})(typeof window !== 'undefined' ? window : global);
