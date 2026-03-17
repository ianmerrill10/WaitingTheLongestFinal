/**
 * @file error-handler.js
 * @brief Global error handler for WaitingTheLongest.com
 * Catches uncaught errors and unhandled promise rejections,
 * logs them, and optionally reports to analytics.
 */
(function() {
    'use strict';

    var errorCount = 0;
    var MAX_ERRORS = 50;

    window.onerror = function(message, source, lineno, colno, error) {
        if (errorCount >= MAX_ERRORS) return;
        errorCount++;

        var errorInfo = {
            type: 'uncaught_error',
            message: message,
            source: source,
            line: lineno,
            column: colno,
            stack: error && error.stack ? error.stack : '',
            url: window.location.href,
            timestamp: new Date().toISOString()
        };

        if (typeof console !== 'undefined' && console.error) {
            console.error('[WTL Error]', errorInfo.message, 'at', errorInfo.source + ':' + errorInfo.line);
        }

        // Report to analytics if available
        if (window.WTLAnalytics) {
            try {
                window.WTLAnalytics.getInstance().trackEvent('CLIENT_ERROR', errorInfo);
            } catch (e) {
                // Don't let error reporting cause more errors
            }
        }

        return false;
    };

    window.addEventListener('unhandledrejection', function(event) {
        if (errorCount >= MAX_ERRORS) return;
        errorCount++;

        var reason = event.reason;
        var errorInfo = {
            type: 'unhandled_rejection',
            message: reason && reason.message ? reason.message : String(reason),
            stack: reason && reason.stack ? reason.stack : '',
            url: window.location.href,
            timestamp: new Date().toISOString()
        };

        if (typeof console !== 'undefined' && console.error) {
            console.error('[WTL Rejection]', errorInfo.message);
        }

        if (window.WTLAnalytics) {
            try {
                window.WTLAnalytics.getInstance().trackEvent('CLIENT_ERROR', errorInfo);
            } catch (e) {
                // Silent
            }
        }
    });
})();
