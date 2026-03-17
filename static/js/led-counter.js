/**
 * @file led-counter.js
 * @brief LED counter display logic for dog wait time visualization
 *
 * PURPOSE:
 * Creates LED-style digital clock displays for showing how long dogs
 * have been waiting for adoption. Supports both wait time (counting up)
 * and countdown modes (for urgent dogs).
 *
 * USAGE:
 * // HTML
 * <div class="led-counter" data-dog-id="dog-uuid"></div>
 *
 * // JavaScript
 * const counter = new LEDCounter(element);
 * counter.setWaitTime({ years: 2, months: 3, days: 15, hours: 8, minutes: 45, seconds: 32 });
 *
 * // Or use static helper
 * LEDCounter.initAll();  // Initialize all .led-counter elements on page
 *
 * DEPENDENCIES:
 * - led-counter.css (for styling)
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

(function(global) {
    'use strict';

    /**
     * LED Counter display modes
     */
    const CounterMode = {
        WAIT_TIME: 'wait-time',      // YY:MM:DD:HH:MM:SS (green, counting up)
        COUNTDOWN: 'countdown',       // DD:HH:MM:SS (red for critical)
        LOADING: 'loading'           // Placeholder state
    };

    /**
     * Segment patterns for digits 0-9
     * Each segment is represented as: a, b, c, d, e, f, g (standard 7-segment layout)
     *
     *    aaa
     *   f   b
     *    ggg
     *   e   c
     *    ddd
     */
    const SEGMENT_PATTERNS = {
        '0': [1, 1, 1, 1, 1, 1, 0],
        '1': [0, 1, 1, 0, 0, 0, 0],
        '2': [1, 1, 0, 1, 1, 0, 1],
        '3': [1, 1, 1, 1, 0, 0, 1],
        '4': [0, 1, 1, 0, 0, 1, 1],
        '5': [1, 0, 1, 1, 0, 1, 1],
        '6': [1, 0, 1, 1, 1, 1, 1],
        '7': [1, 1, 1, 0, 0, 0, 0],
        '8': [1, 1, 1, 1, 1, 1, 1],
        '9': [1, 1, 1, 1, 0, 1, 1],
        '-': [0, 0, 0, 0, 0, 0, 1]
    };

    /**
     * Counter group labels
     */
    const WAIT_TIME_LABELS = ['YR', 'MO', 'DY', 'HR', 'MN', 'SC'];
    const COUNTDOWN_LABELS = ['DY', 'HR', 'MN', 'SC'];

    /**
     * LEDCounter class
     * Creates and manages a single LED counter display
     */
    class LEDCounter {
        /**
         * Create a new LED counter
         * @param {HTMLElement} element Container element
         * @param {Object} options Configuration options
         */
        constructor(element, options = {}) {
            this.element = element;
            this.options = {
                mode: options.mode || CounterMode.WAIT_TIME,
                showLabels: options.showLabels !== false,
                animate: options.animate !== false,
                useSegments: options.useSegments !== false
            };

            this.mode = this.options.mode;
            this.currentValue = null;
            this.isCritical = false;

            this._init();
        }

        /**
         * Initialize the counter display
         */
        _init() {
            this.element.classList.add('led-counter');
            this.element.innerHTML = '';

            // Create container
            this.container = document.createElement('div');
            this.container.className = 'led-counter-container';
            this.element.appendChild(this.container);

            // Create digit groups
            this.groups = [];
            const groupCount = this.mode === CounterMode.COUNTDOWN ? 4 : 6;
            const labels = this.mode === CounterMode.COUNTDOWN ? COUNTDOWN_LABELS : WAIT_TIME_LABELS;

            for (let i = 0; i < groupCount; i++) {
                const group = this._createDigitGroup(labels[i], i);
                this.groups.push(group);
                this.container.appendChild(group.element);

                // Add separator (colon) between groups except after last
                if (i < groupCount - 1) {
                    const separator = this._createSeparator();
                    this.container.appendChild(separator);
                }
            }

            // Set initial loading state
            this._setLoading();

            // Store reference in element for external access
            this.element._ledCounter = this;
        }

        /**
         * Create a digit group (2 digits with label)
         */
        _createDigitGroup(label, index) {
            const group = document.createElement('div');
            group.className = 'led-digit-group';

            // Create digits container
            const digitsContainer = document.createElement('div');
            digitsContainer.className = 'led-digits';

            // Create two digit displays
            const digit1 = this._createDigit();
            const digit2 = this._createDigit();

            digitsContainer.appendChild(digit1.element);
            digitsContainer.appendChild(digit2.element);
            group.appendChild(digitsContainer);

            // Create label if enabled
            if (this.options.showLabels) {
                const labelEl = document.createElement('div');
                labelEl.className = 'led-label';
                labelEl.textContent = label;
                group.appendChild(labelEl);
            }

            return {
                element: group,
                digit1: digit1,
                digit2: digit2,
                label: label
            };
        }

        /**
         * Create a single digit display
         */
        _createDigit() {
            const digit = document.createElement('div');
            digit.className = 'led-digit';

            if (this.options.useSegments) {
                // Create 7-segment display
                const segments = ['a', 'b', 'c', 'd', 'e', 'f', 'g'];
                segments.forEach(seg => {
                    const segment = document.createElement('div');
                    segment.className = `led-segment led-segment-${seg}`;
                    digit.appendChild(segment);
                });
            }

            return {
                element: digit,
                value: null
            };
        }

        /**
         * Create separator (colon) between groups
         */
        _createSeparator() {
            const separator = document.createElement('div');
            separator.className = 'led-separator';

            // Create two dots for colon
            const dot1 = document.createElement('div');
            dot1.className = 'led-separator-dot';
            const dot2 = document.createElement('div');
            dot2.className = 'led-separator-dot';

            separator.appendChild(dot1);
            separator.appendChild(dot2);

            return separator;
        }

        /**
         * Set the counter to loading state
         */
        _setLoading() {
            this.groups.forEach(group => {
                this._setDigitValue(group.digit1, '-');
                this._setDigitValue(group.digit2, '-');
            });
            this.element.classList.add('loading');
        }

        /**
         * Set a digit's value
         */
        _setDigitValue(digit, value) {
            const newValue = String(value);

            if (digit.value === newValue) {
                return; // No change
            }

            const oldValue = digit.value;
            digit.value = newValue;

            if (this.options.useSegments) {
                // Update 7-segment display
                const pattern = SEGMENT_PATTERNS[newValue] || SEGMENT_PATTERNS['-'];
                const segments = digit.element.querySelectorAll('.led-segment');

                segments.forEach((segment, index) => {
                    if (pattern[index]) {
                        segment.classList.add('active');
                    } else {
                        segment.classList.remove('active');
                    }
                });
            } else {
                // Simple text display with font
                digit.element.textContent = newValue;
            }

            // Animate if enabled and value changed
            if (this.options.animate && oldValue !== null && oldValue !== newValue) {
                digit.element.classList.add('changing');
                setTimeout(() => {
                    digit.element.classList.remove('changing');
                }, 200);
            }
        }

        /**
         * Set wait time values (YY:MM:DD:HH:MM:SS)
         * @param {Object} data Wait time data
         */
        setWaitTime(data) {
            this.element.classList.remove('loading');
            this.element.classList.remove('countdown');
            this.element.classList.remove('critical');
            this.element.classList.add('wait-time');

            // Ensure we have 6 groups for wait time
            if (this.groups.length !== 6) {
                this.mode = CounterMode.WAIT_TIME;
                this._init();
            }

            const values = [
                data.years || 0,
                data.months || 0,
                data.days || 0,
                data.hours || 0,
                data.minutes || 0,
                data.seconds || 0
            ];

            this._updateDigits(values);
            this.currentValue = data;
            this.isCritical = false;
        }

        /**
         * Set countdown values (DD:HH:MM:SS)
         * @param {Object} data Countdown data
         */
        setCountdown(data) {
            this.element.classList.remove('loading');
            this.element.classList.remove('wait-time');
            this.element.classList.add('countdown');

            // Handle critical state
            if (data.is_critical) {
                this.element.classList.add('critical');
                this.isCritical = true;
            } else {
                this.element.classList.remove('critical');
                this.isCritical = false;
            }

            // Ensure we have 4 groups for countdown
            if (this.groups.length !== 4) {
                this.mode = CounterMode.COUNTDOWN;
                this._init();
            }

            const values = [
                data.days || 0,
                data.hours || 0,
                data.minutes || 0,
                data.seconds || 0
            ];

            this._updateDigits(values);
            this.currentValue = data;
        }

        /**
         * Update digit displays with values
         */
        _updateDigits(values) {
            values.forEach((value, groupIndex) => {
                if (groupIndex < this.groups.length) {
                    const group = this.groups[groupIndex];
                    const padded = String(value).padStart(2, '0');
                    this._setDigitValue(group.digit1, padded[0]);
                    this._setDigitValue(group.digit2, padded[1]);
                }
            });
        }

        /**
         * Get current counter value
         * @returns {Object} Current value object
         */
        getValue() {
            return this.currentValue;
        }

        /**
         * Check if counter is in critical state
         * @returns {boolean} True if critical
         */
        isCriticalState() {
            return this.isCritical;
        }

        /**
         * Destroy the counter and clean up
         */
        destroy() {
            if (this.element._ledCounter) {
                delete this.element._ledCounter;
            }
            this.element.innerHTML = '';
            this.element.classList.remove('led-counter', 'loading', 'wait-time', 'countdown', 'critical');
        }

        // ====================================================================
        // STATIC METHODS
        // ====================================================================

        /**
         * Get or create a LEDCounter for an element
         * @param {HTMLElement} element Target element
         * @param {Object} options Options for new counter
         * @returns {LEDCounter} Counter instance
         */
        static getOrCreate(element, options = {}) {
            if (element._ledCounter) {
                return element._ledCounter;
            }
            return new LEDCounter(element, options);
        }

        /**
         * Initialize all LED counter elements on the page
         * @param {Object} options Default options
         * @returns {LEDCounter[]} Array of initialized counters
         */
        static initAll(options = {}) {
            const elements = document.querySelectorAll('.led-counter');
            const counters = [];

            elements.forEach(element => {
                if (!element._ledCounter) {
                    const counter = new LEDCounter(element, options);
                    counters.push(counter);
                } else {
                    counters.push(element._ledCounter);
                }
            });

            console.log(`[LEDCounter] Initialized ${counters.length} counters`);
            return counters;
        }

        /**
         * Parse formatted time string to components
         * @param {string} formatted Time string (e.g., "02:03:15:08:45:32")
         * @param {string} mode Counter mode
         * @returns {Object} Parsed components
         */
        static parseFormatted(formatted, mode = CounterMode.WAIT_TIME) {
            const parts = formatted.split(':').map(p => parseInt(p, 10));

            if (mode === CounterMode.COUNTDOWN) {
                return {
                    days: parts[0] || 0,
                    hours: parts[1] || 0,
                    minutes: parts[2] || 0,
                    seconds: parts[3] || 0
                };
            } else {
                return {
                    years: parts[0] || 0,
                    months: parts[1] || 0,
                    days: parts[2] || 0,
                    hours: parts[3] || 0,
                    minutes: parts[4] || 0,
                    seconds: parts[5] || 0
                };
            }
        }

        /**
         * Format time components to string
         * @param {Object} components Time components
         * @param {string} mode Counter mode
         * @returns {string} Formatted time string
         */
        static formatComponents(components, mode = CounterMode.WAIT_TIME) {
            const pad = (n) => String(n).padStart(2, '0');

            if (mode === CounterMode.COUNTDOWN) {
                return [
                    pad(components.days || 0),
                    pad(components.hours || 0),
                    pad(components.minutes || 0),
                    pad(components.seconds || 0)
                ].join(':');
            } else {
                return [
                    pad(components.years || 0),
                    pad(components.months || 0),
                    pad(components.days || 0),
                    pad(components.hours || 0),
                    pad(components.minutes || 0),
                    pad(components.seconds || 0)
                ].join(':');
            }
        }
    }

    // Export to global scope
    global.LEDCounter = LEDCounter;
    global.LED_CounterMode = CounterMode;

    // Auto-initialize on DOM ready
    if (typeof document !== 'undefined') {
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => {
                LEDCounter.initAll();
            });
        } else {
            // DOM already loaded
            LEDCounter.initAll();
        }
    }

})(typeof window !== 'undefined' ? window : global);
