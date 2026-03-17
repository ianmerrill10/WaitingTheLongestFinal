/**
 * @file sw-notifications.js
 * @brief Service Worker for push notification handling
 *
 * PURPOSE:
 * Handles background push notifications when the app is closed or in background:
 * - Receives push events from FCM/Web Push
 * - Displays native notifications
 * - Handles notification clicks and actions
 * - Manages notification badges
 *
 * USAGE:
 * Registered automatically by notifications.js
 *
 * EVENTS HANDLED:
 * - push: Incoming push messages
 * - notificationclick: User clicks notification
 * - notificationclose: User dismisses notification
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

'use strict';

// Service Worker version for cache busting
const SW_VERSION = '1.0.0';
const CACHE_NAME = 'wtl-notifications-v1';

// Default notification options
const DEFAULT_NOTIFICATION = {
    icon: '/static/images/logo-icon.png',
    badge: '/static/images/badge.png',
    vibrate: [200, 100, 200],
    requireInteraction: false,
    tag: 'wtl-notification'
};

// Critical notification options (always show, require interaction)
const CRITICAL_NOTIFICATION = {
    ...DEFAULT_NOTIFICATION,
    requireInteraction: true,
    vibrate: [500, 200, 500, 200, 500],
    tag: 'wtl-critical'
};

/**
 * Install event - cache essential assets
 */
self.addEventListener('install', (event) => {
    console.log('[SW] Installing service worker v' + SW_VERSION);

    event.waitUntil(
        caches.open(CACHE_NAME).then((cache) => {
            return cache.addAll([
                '/static/images/logo-icon.png',
                '/static/images/badge.png'
            ]).catch((error) => {
                // Non-critical if images fail to cache
                console.warn('[SW] Cache addAll failed:', error);
            });
        })
    );

    // Activate immediately
    self.skipWaiting();
});

/**
 * Activate event - clean up old caches
 */
self.addEventListener('activate', (event) => {
    console.log('[SW] Activating service worker v' + SW_VERSION);

    event.waitUntil(
        caches.keys().then((cacheNames) => {
            return Promise.all(
                cacheNames
                    .filter((name) => name.startsWith('wtl-') && name !== CACHE_NAME)
                    .map((name) => caches.delete(name))
            );
        })
    );

    // Claim all clients immediately
    self.clients.claim();
});

/**
 * Push event - handle incoming push notifications
 */
self.addEventListener('push', (event) => {
    console.log('[SW] Push received:', event);

    if (!event.data) {
        console.warn('[SW] Push event with no data');
        return;
    }

    let data;
    try {
        data = event.data.json();
    } catch (e) {
        // Fallback for plain text
        data = {
            notification: {
                title: 'WaitingTheLongest',
                body: event.data.text()
            }
        };
    }

    const notification = data.notification || {};
    const payload = data.data || {};

    // Determine notification options based on type
    const isCritical = payload.type === 'critical_alert' ||
                       payload.type === 'high_urgency' ||
                       payload.priority === '1';

    const options = {
        ...(isCritical ? CRITICAL_NOTIFICATION : DEFAULT_NOTIFICATION),
        body: notification.body || payload.body || '',
        icon: notification.icon || payload.icon || DEFAULT_NOTIFICATION.icon,
        badge: notification.badge || payload.badge || DEFAULT_NOTIFICATION.badge,
        image: notification.image || payload.image,
        tag: payload.tag || (isCritical ? 'wtl-critical' : 'wtl-notification'),
        data: {
            ...payload,
            notification_id: payload.notification_id,
            action_url: payload.action_url || notification.click_action
        },
        actions: buildActions(payload.type, payload)
    };

    // Add timestamp for sorting
    if (notification.timestamp) {
        options.timestamp = new Date(notification.timestamp).getTime();
    }

    const title = notification.title || payload.title || 'WaitingTheLongest';

    event.waitUntil(
        self.registration.showNotification(title, options)
    );
});

/**
 * Build notification actions based on type
 */
function buildActions(type, payload) {
    const actions = [];

    switch (type) {
        case 'critical_alert':
        case 'high_urgency':
            actions.push(
                { action: 'view', title: 'View Dog', icon: '/static/images/icon-view.png' },
                { action: 'share', title: 'Share', icon: '/static/images/icon-share.png' }
            );
            break;

        case 'foster_needed_urgent':
            actions.push(
                { action: 'apply', title: 'Apply to Foster', icon: '/static/images/icon-foster.png' },
                { action: 'view', title: 'View Details', icon: '/static/images/icon-view.png' }
            );
            break;

        case 'perfect_match':
        case 'good_match':
            actions.push(
                { action: 'view', title: 'View Match', icon: '/static/images/icon-view.png' },
                { action: 'dismiss', title: 'Not Interested', icon: '/static/images/icon-dismiss.png' }
            );
            break;

        case 'dog_update':
            actions.push(
                { action: 'view', title: 'View Update', icon: '/static/images/icon-view.png' }
            );
            break;

        default:
            actions.push(
                { action: 'view', title: 'View', icon: '/static/images/icon-view.png' }
            );
    }

    return actions;
}

/**
 * Notification click event - handle user clicking the notification
 */
self.addEventListener('notificationclick', (event) => {
    console.log('[SW] Notification clicked:', event);

    const notification = event.notification;
    const data = notification.data || {};
    const action = event.action;

    // Close the notification
    notification.close();

    // Build the target URL
    let targetUrl = '/';

    if (action === 'view' || !action) {
        // Default action - go to action URL or relevant page
        if (data.action_url) {
            targetUrl = data.action_url;
        } else if (data.dog_id) {
            targetUrl = '/dogs/' + data.dog_id;
        } else if (data.type === 'success_story') {
            targetUrl = '/blog/success-stories';
        }
    } else if (action === 'share' && data.dog_id) {
        targetUrl = '/dogs/' + data.dog_id + '?share=true';
    } else if (action === 'apply' && data.dog_id) {
        targetUrl = '/foster/apply?dog_id=' + data.dog_id;
    } else if (action === 'dismiss') {
        // Just track the dismissal, don't navigate
        trackNotificationAction(data.notification_id, 'dismissed');
        return;
    }

    // Track the click
    trackNotificationAction(data.notification_id, action || 'clicked');

    event.waitUntil(
        // Try to focus an existing window or open a new one
        clients.matchAll({ type: 'window', includeUncontrolled: true })
            .then((windowClients) => {
                // Check if there's already a window open
                for (const client of windowClients) {
                    if (client.url.includes(self.location.origin) && 'focus' in client) {
                        client.postMessage({
                            type: 'NOTIFICATION_CLICKED',
                            notification: {
                                ...data,
                                action: action
                            }
                        });
                        client.navigate(targetUrl);
                        return client.focus();
                    }
                }
                // No window found, open a new one
                if (clients.openWindow) {
                    return clients.openWindow(targetUrl);
                }
            })
    );
});

/**
 * Notification close event - handle user dismissing notification
 */
self.addEventListener('notificationclose', (event) => {
    console.log('[SW] Notification closed:', event);

    const notification = event.notification;
    const data = notification.data || {};

    // Track the dismissal
    trackNotificationAction(data.notification_id, 'dismissed');
});

/**
 * Track notification action to server
 */
function trackNotificationAction(notificationId, action) {
    if (!notificationId) return;

    const url = `/api/notifications/${notificationId}/clicked`;

    fetch(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ action }),
        credentials: 'include'
    }).catch((error) => {
        console.warn('[SW] Failed to track notification action:', error);
    });
}

/**
 * Message event - handle messages from the main page
 */
self.addEventListener('message', (event) => {
    console.log('[SW] Message received:', event.data);

    if (event.data.type === 'SKIP_WAITING') {
        self.skipWaiting();
    }
});

/**
 * Background sync event - retry failed notification tracking
 */
self.addEventListener('sync', (event) => {
    if (event.tag === 'notification-sync') {
        console.log('[SW] Background sync triggered');
        // Could implement retry logic for failed tracking here
    }
});

// Log service worker startup
console.log('[SW] Service worker loaded v' + SW_VERSION);
