/**
 * @file websocket-client.js
 * @brief JavaScript WebSocket client for real-time dog wait time updates
 *
 * PURPOSE:
 * Provides a client-side WebSocket connection to receive real-time updates
 * for dog wait times, countdowns, and urgent alerts. Automatically updates
 * DOM elements with class "led-counter" based on data-dog-id attribute.
 *
 * USAGE:
 * // Initialize and connect
 * const client = new WaitTimeWebSocketClient();
 * client.connect();
 *
 * // Subscribe to a dog
 * client.subscribeToDog('dog-uuid');
 *
 * // DOM elements are updated automatically
 * // <div class="led-counter" data-dog-id="dog-uuid"></div>
 *
 * DEPENDENCIES:
 * - led-counter.js (for LED display rendering)
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

(function(global) {
    'use strict';

    /**
     * Message types matching server-side MessageType enum
     */
    const MessageType = {
        // Client -> Server
        SUBSCRIBE_DOG: 'SUBSCRIBE_DOG',
        UNSUBSCRIBE_DOG: 'UNSUBSCRIBE_DOG',
        SUBSCRIBE_SHELTER: 'SUBSCRIBE_SHELTER',
        UNSUBSCRIBE_SHELTER: 'UNSUBSCRIBE_SHELTER',
        SUBSCRIBE_URGENT: 'SUBSCRIBE_URGENT',

        // Server -> Client
        WAIT_TIME_UPDATE: 'WAIT_TIME_UPDATE',
        COUNTDOWN_UPDATE: 'COUNTDOWN_UPDATE',
        DOG_STATUS_CHANGE: 'DOG_STATUS_CHANGE',
        URGENT_ALERT: 'URGENT_ALERT',
        CONNECTION_ACK: 'CONNECTION_ACK',
        ERROR: 'ERROR'
    };

    /**
     * Connection states
     */
    const ConnectionState = {
        DISCONNECTED: 'disconnected',
        CONNECTING: 'connecting',
        CONNECTED: 'connected',
        RECONNECTING: 'reconnecting'
    };

    /**
     * WaitTimeWebSocketClient
     * WebSocket client for real-time dog wait time updates
     */
    class WaitTimeWebSocketClient {
        /**
         * Create a new WebSocket client
         * @param {Object} options Configuration options
         * @param {string} options.url WebSocket server URL (default: auto-detect)
         * @param {boolean} options.autoReconnect Enable auto-reconnect (default: true)
         * @param {number} options.reconnectDelay Initial reconnect delay in ms (default: 1000)
         * @param {number} options.maxReconnectDelay Maximum reconnect delay in ms (default: 30000)
         * @param {boolean} options.autoSubscribeFromDOM Auto-subscribe based on DOM elements (default: true)
         */
        constructor(options = {}) {
            this.options = {
                url: options.url || this._getDefaultUrl(),
                autoReconnect: options.autoReconnect !== false,
                reconnectDelay: options.reconnectDelay || 1000,
                maxReconnectDelay: options.maxReconnectDelay || 30000,
                autoSubscribeFromDOM: options.autoSubscribeFromDOM !== false
            };

            this.ws = null;
            this.connectionId = null;
            this.state = ConnectionState.DISCONNECTED;
            this.reconnectAttempts = 0;
            this.reconnectTimer = null;

            // Subscriptions tracking
            this.dogSubscriptions = new Set();
            this.shelterSubscriptions = new Set();
            this.urgentSubscribed = false;

            // Event callbacks
            this.callbacks = {
                onConnect: [],
                onDisconnect: [],
                onWaitTimeUpdate: [],
                onCountdownUpdate: [],
                onDogStatusChange: [],
                onUrgentAlert: [],
                onError: []
            };

            // Bind methods
            this._onOpen = this._onOpen.bind(this);
            this._onMessage = this._onMessage.bind(this);
            this._onClose = this._onClose.bind(this);
            this._onError = this._onError.bind(this);
        }

        /**
         * Get default WebSocket URL based on current page location
         */
        _getDefaultUrl() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const host = window.location.host;
            return `${protocol}//${host}/ws/dogs`;
        }

        /**
         * Connect to the WebSocket server
         * @returns {Promise} Resolves when connected
         */
        connect() {
            return new Promise((resolve, reject) => {
                if (this.state === ConnectionState.CONNECTED) {
                    resolve();
                    return;
                }

                this.state = ConnectionState.CONNECTING;

                try {
                    this.ws = new WebSocket(this.options.url);
                    this.ws.onopen = (event) => {
                        this._onOpen(event);
                        resolve();
                    };
                    this.ws.onmessage = this._onMessage;
                    this.ws.onclose = this._onClose;
                    this.ws.onerror = (event) => {
                        this._onError(event);
                        if (this.state === ConnectionState.CONNECTING) {
                            reject(new Error('WebSocket connection failed'));
                        }
                    };
                } catch (error) {
                    this.state = ConnectionState.DISCONNECTED;
                    reject(error);
                }
            });
        }

        /**
         * Disconnect from the WebSocket server
         */
        disconnect() {
            this.options.autoReconnect = false;
            this._clearReconnectTimer();

            if (this.ws) {
                this.ws.close(1000, 'Client disconnecting');
                this.ws = null;
            }

            this.state = ConnectionState.DISCONNECTED;
            this.dogSubscriptions.clear();
            this.shelterSubscriptions.clear();
            this.urgentSubscribed = false;
        }

        /**
         * Subscribe to a dog's wait time updates
         * @param {string} dogId The dog's unique identifier
         */
        subscribeToDog(dogId) {
            if (!dogId) return;

            this.dogSubscriptions.add(dogId);

            if (this.state === ConnectionState.CONNECTED) {
                this._send({
                    type: MessageType.SUBSCRIBE_DOG,
                    dog_id: dogId
                });
            }
        }

        /**
         * Unsubscribe from a dog's updates
         * @param {string} dogId The dog's unique identifier
         */
        unsubscribeFromDog(dogId) {
            if (!dogId) return;

            this.dogSubscriptions.delete(dogId);

            if (this.state === ConnectionState.CONNECTED) {
                this._send({
                    type: MessageType.UNSUBSCRIBE_DOG,
                    dog_id: dogId
                });
            }
        }

        /**
         * Subscribe to a shelter's updates
         * @param {string} shelterId The shelter's unique identifier
         */
        subscribeToShelter(shelterId) {
            if (!shelterId) return;

            this.shelterSubscriptions.add(shelterId);

            if (this.state === ConnectionState.CONNECTED) {
                this._send({
                    type: MessageType.SUBSCRIBE_SHELTER,
                    shelter_id: shelterId
                });
            }
        }

        /**
         * Unsubscribe from a shelter's updates
         * @param {string} shelterId The shelter's unique identifier
         */
        unsubscribeFromShelter(shelterId) {
            if (!shelterId) return;

            this.shelterSubscriptions.delete(shelterId);

            if (this.state === ConnectionState.CONNECTED) {
                this._send({
                    type: MessageType.UNSUBSCRIBE_SHELTER,
                    shelter_id: shelterId
                });
            }
        }

        /**
         * Subscribe to urgent dog alerts
         */
        subscribeToUrgent() {
            this.urgentSubscribed = true;

            if (this.state === ConnectionState.CONNECTED) {
                this._send({
                    type: MessageType.SUBSCRIBE_URGENT
                });
            }
        }

        /**
         * Register an event callback
         * @param {string} event Event name (connect, disconnect, waitTimeUpdate, etc.)
         * @param {Function} callback Callback function
         */
        on(event, callback) {
            const eventName = 'on' + event.charAt(0).toUpperCase() + event.slice(1);
            if (this.callbacks[eventName]) {
                this.callbacks[eventName].push(callback);
            }
        }

        /**
         * Remove an event callback
         * @param {string} event Event name
         * @param {Function} callback Callback function to remove
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
         * Get connection state
         * @returns {string} Current connection state
         */
        getState() {
            return this.state;
        }

        /**
         * Get connection ID
         * @returns {string|null} Connection ID if connected
         */
        getConnectionId() {
            return this.connectionId;
        }

        // ====================================================================
        // PRIVATE METHODS
        // ====================================================================

        _onOpen(event) {
            this.state = ConnectionState.CONNECTED;
            this.reconnectAttempts = 0;
            console.log('[WaitTimeWS] Connected to server');

            // Resubscribe to previous subscriptions
            this._resubscribe();

            // Auto-subscribe from DOM if enabled
            if (this.options.autoSubscribeFromDOM) {
                this._autoSubscribeFromDOM();
            }

            // Notify listeners
            this._emit('onConnect', { connectionId: this.connectionId });
        }

        _onMessage(event) {
            try {
                const message = JSON.parse(event.data);
                this._handleMessage(message);
            } catch (error) {
                console.error('[WaitTimeWS] Failed to parse message:', error);
            }
        }

        _handleMessage(message) {
            switch (message.type) {
                case MessageType.CONNECTION_ACK:
                    if (message.payload && message.payload.connection_id) {
                        this.connectionId = message.payload.connection_id;
                        console.log('[WaitTimeWS] Connection acknowledged:', this.connectionId);
                    }
                    break;

                case MessageType.WAIT_TIME_UPDATE:
                    this._handleWaitTimeUpdate(message);
                    break;

                case MessageType.COUNTDOWN_UPDATE:
                    this._handleCountdownUpdate(message);
                    break;

                case MessageType.DOG_STATUS_CHANGE:
                    this._handleDogStatusChange(message);
                    break;

                case MessageType.URGENT_ALERT:
                    this._handleUrgentAlert(message);
                    break;

                case MessageType.ERROR:
                    console.error('[WaitTimeWS] Server error:', message.payload);
                    this._emit('onError', message.payload);
                    break;

                default:
                    console.log('[WaitTimeWS] Unknown message type:', message.type);
            }
        }

        _handleWaitTimeUpdate(message) {
            const { dog_id, payload } = message;

            // Update DOM elements
            this._updateDOMElements(dog_id, payload, 'wait-time');

            // Notify listeners
            this._emit('onWaitTimeUpdate', { dogId: dog_id, waitTime: payload });
        }

        _handleCountdownUpdate(message) {
            const { dog_id, payload } = message;

            // Update DOM elements
            this._updateDOMElements(dog_id, payload, 'countdown');

            // Notify listeners
            this._emit('onCountdownUpdate', { dogId: dog_id, countdown: payload });
        }

        _handleDogStatusChange(message) {
            const { dog_id, payload } = message;
            console.log('[WaitTimeWS] Dog status changed:', dog_id, payload.new_status);

            // Notify listeners
            this._emit('onDogStatusChange', { dogId: dog_id, status: payload });
        }

        _handleUrgentAlert(message) {
            const { dog_id, payload } = message;
            console.warn('[WaitTimeWS] URGENT ALERT:', dog_id, payload.urgency_level);

            // Notify listeners
            this._emit('onUrgentAlert', { dogId: dog_id, alert: payload });
        }

        _updateDOMElements(dogId, data, mode) {
            const elements = document.querySelectorAll(
                `.led-counter[data-dog-id="${dogId}"]`
            );

            elements.forEach(element => {
                // Use LEDCounter if available
                if (typeof LEDCounter !== 'undefined') {
                    const counter = LEDCounter.getOrCreate(element);
                    if (mode === 'countdown') {
                        counter.setCountdown(data);
                    } else {
                        counter.setWaitTime(data);
                    }
                } else {
                    // Fallback: simple text update
                    element.textContent = data.formatted || '';
                    if (mode === 'countdown' && data.is_critical) {
                        element.classList.add('critical');
                    } else {
                        element.classList.remove('critical');
                    }
                }
            });
        }

        _onClose(event) {
            const wasConnected = this.state === ConnectionState.CONNECTED;
            this.state = ConnectionState.DISCONNECTED;
            this.connectionId = null;

            console.log('[WaitTimeWS] Connection closed:', event.code, event.reason);

            // Notify listeners
            this._emit('onDisconnect', { code: event.code, reason: event.reason });

            // Auto-reconnect if enabled and not a clean close
            if (this.options.autoReconnect && event.code !== 1000) {
                this._scheduleReconnect();
            }
        }

        _onError(event) {
            console.error('[WaitTimeWS] WebSocket error:', event);
            this._emit('onError', { message: 'WebSocket error' });
        }

        _send(message) {
            if (this.ws && this.state === ConnectionState.CONNECTED) {
                this.ws.send(JSON.stringify(message));
            }
        }

        _emit(eventName, data) {
            if (this.callbacks[eventName]) {
                this.callbacks[eventName].forEach(callback => {
                    try {
                        callback(data);
                    } catch (error) {
                        console.error('[WaitTimeWS] Callback error:', error);
                    }
                });
            }
        }

        _scheduleReconnect() {
            this._clearReconnectTimer();

            this.state = ConnectionState.RECONNECTING;
            this.reconnectAttempts++;

            // Exponential backoff with jitter
            const delay = Math.min(
                this.options.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1),
                this.options.maxReconnectDelay
            );
            const jitter = delay * 0.2 * Math.random();
            const actualDelay = delay + jitter;

            console.log(`[WaitTimeWS] Reconnecting in ${Math.round(actualDelay)}ms (attempt ${this.reconnectAttempts})`);

            this.reconnectTimer = setTimeout(() => {
                this.connect().catch(error => {
                    console.error('[WaitTimeWS] Reconnection failed:', error);
                });
            }, actualDelay);
        }

        _clearReconnectTimer() {
            if (this.reconnectTimer) {
                clearTimeout(this.reconnectTimer);
                this.reconnectTimer = null;
            }
        }

        _resubscribe() {
            // Resubscribe to dogs
            this.dogSubscriptions.forEach(dogId => {
                this._send({
                    type: MessageType.SUBSCRIBE_DOG,
                    dog_id: dogId
                });
            });

            // Resubscribe to shelters
            this.shelterSubscriptions.forEach(shelterId => {
                this._send({
                    type: MessageType.SUBSCRIBE_SHELTER,
                    shelter_id: shelterId
                });
            });

            // Resubscribe to urgent
            if (this.urgentSubscribed) {
                this._send({
                    type: MessageType.SUBSCRIBE_URGENT
                });
            }
        }

        _autoSubscribeFromDOM() {
            const elements = document.querySelectorAll('.led-counter[data-dog-id]');
            elements.forEach(element => {
                const dogId = element.getAttribute('data-dog-id');
                if (dogId) {
                    this.subscribeToDog(dogId);
                }
            });

            console.log(`[WaitTimeWS] Auto-subscribed to ${elements.length} dogs from DOM`);
        }
    }

    /**
     * UrgentWebSocketClient
     * Dedicated client for urgent dog alerts
     */
    class UrgentWebSocketClient extends WaitTimeWebSocketClient {
        constructor(options = {}) {
            super({
                ...options,
                url: options.url || UrgentWebSocketClient._getDefaultUrl()
            });
        }

        static _getDefaultUrl() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const host = window.location.host;
            return `${protocol}//${host}/ws/urgent`;
        }
    }

    // Export to global scope
    global.WaitTimeWebSocketClient = WaitTimeWebSocketClient;
    global.UrgentWebSocketClient = UrgentWebSocketClient;
    global.WS_MessageType = MessageType;
    global.WS_ConnectionState = ConnectionState;

})(typeof window !== 'undefined' ? window : global);
