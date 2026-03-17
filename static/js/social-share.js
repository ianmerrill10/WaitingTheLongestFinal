/**
 * @file social-share.js
 * @brief Social sharing utility for sharing dog profiles
 * PURPOSE: Enables sharing dog profiles across multiple social platforms and channels
 * USAGE: Call SocialShare.share(platform, dogData) to initiate share
 * DEPENDENCIES: None (vanilla JavaScript)
 * @author Frontend Build - WaitingTheLongest Build System
 * @date 2024-01-28
 */

class SocialShare {
  constructor() {
    this.baseUrl = 'https://waitingthelongest.com';
    this.tooltipTimeout = null;
  }

  /**
   * Initialize social share functionality
   */
  static init() {
    if (SocialShare.instance) {
      return SocialShare.instance;
    }
    SocialShare.instance = new SocialShare();
    return SocialShare.instance;
  }

  /**
   * Main share method
   * @param {string} platform - Share platform (facebook, twitter, email, copy, text)
   * @param {object} dogData - Dog profile data
   */
  async share(platform, dogData) {
    if (!dogData || !dogData.id) {
      console.error('SocialShare: Invalid dog data');
      return;
    }

    try {
      switch (platform.toLowerCase()) {
        case 'facebook':
          this.shareFacebook(dogData);
          break;
        case 'twitter':
          this.shareTwitter(dogData);
          break;
        case 'email':
          this.shareEmail(dogData);
          break;
        case 'copy':
          await this.shareCopyLink(dogData);
          break;
        case 'text':
          this.shareText(dogData);
          break;
        default:
          console.warn(`SocialShare: Unknown platform "${platform}"`);
      }

      // Track the share
      await this.trackShare(dogData.id, platform);
    } catch (error) {
      console.error(`SocialShare: Error sharing to ${platform}`, error);
    }
  }

  /**
   * Share to Facebook
   */
  shareFacebook(dogData) {
    const shareUrl = this.generateShareUrl(dogData.id);
    const facebookUrl = `https://www.facebook.com/sharer/sharer.php?u=${encodeURIComponent(shareUrl)}`;
    window.open(facebookUrl, 'facebook-share', 'width=600,height=400');
  }

  /**
   * Share to Twitter
   */
  shareTwitter(dogData) {
    const shareUrl = this.generateShareUrl(dogData.id);
    const shareText = this.generateShareText(dogData);
    const twitterUrl = `https://twitter.com/intent/tweet?url=${encodeURIComponent(shareUrl)}&text=${encodeURIComponent(shareText)}`;
    window.open(twitterUrl, 'twitter-share', 'width=600,height=400');
  }

  /**
   * Share via Email
   */
  shareEmail(dogData) {
    const shareUrl = this.generateShareUrl(dogData.id);
    const subject = `Help ${this.escapeText(dogData.name)} find a home`;
    const body = this.generateEmailBody(dogData, shareUrl);
    const mailtoUrl = `mailto:?subject=${encodeURIComponent(subject)}&body=${encodeURIComponent(body)}`;
    window.location.href = mailtoUrl;
  }

  /**
   * Copy share link to clipboard
   */
  async shareCopyLink(dogData) {
    const shareUrl = this.generateShareUrl(dogData.id);

    try {
      await navigator.clipboard.writeText(shareUrl);
      this.showTooltip('Copied!');
    } catch (error) {
      console.error('SocialShare: Failed to copy to clipboard', error);
      // Fallback: use execCommand
      this.fallbackCopyToClipboard(shareUrl);
    }
  }

  /**
   * Fallback copy to clipboard using execCommand
   */
  fallbackCopyToClipboard(text) {
    const textArea = document.createElement('textarea');
    textArea.value = text;
    document.body.appendChild(textArea);
    textArea.select();

    try {
      document.execCommand('copy');
      this.showTooltip('Copied!');
    } catch (error) {
      console.error('SocialShare: Fallback copy failed', error);
    }

    document.body.removeChild(textArea);
  }

  /**
   * Share via SMS (mobile)
   */
  shareText(dogData) {
    const shareUrl = this.generateShareUrl(dogData.id);
    const shareText = this.generateShareText(dogData);
    const smsUrl = `sms:?body=${encodeURIComponent(`${shareText}\n${shareUrl}`)}`;
    window.location.href = smsUrl;
  }

  /**
   * Generate compelling share text
   */
  generateShareText(dogData) {
    if (!dogData.name) return 'Help a dog find a home!';

    let text = `Meet ${dogData.name}!`;

    if (dogData.waitTime) {
      text += ` They've been waiting ${dogData.waitTime} to find their forever home.`;
    } else {
      text += ` They're waiting to find their forever home.`;
    }

    if (dogData.shelter && dogData.shelter.name) {
      text += ` Currently at ${dogData.shelter.name}.`;
    }

    text += ' I can\'t adopt, but I can share! ❤️';

    return text;
  }

  /**
   * Generate email body
   */
  generateEmailBody(dogData, shareUrl) {
    let body = `I'd like to share a dog profile with you:\n\n`;
    body += `Name: ${dogData.name || 'Unknown'}\n`;

    if (dogData.breed) {
      body += `Breed: ${dogData.breed}\n`;
    }

    if (dogData.waitTime) {
      body += `Wait Time: ${dogData.waitTime}\n`;
    }

    if (dogData.shelter && dogData.shelter.name) {
      body += `Shelter: ${dogData.shelter.name}\n`;
    }

    if (dogData.description) {
      body += `\nAbout: ${dogData.description}\n`;
    }

    body += `\nView Profile: ${shareUrl}\n\n`;
    body += `Help ${dogData.name} find a home by sharing this profile!\n`;
    body += `"I can't adopt, but I can share!" ❤️`;

    return body;
  }

  /**
   * Generate shareable URL
   */
  generateShareUrl(dogId) {
    return `${this.baseUrl}/dog.html?id=${encodeURIComponent(dogId)}`;
  }

  /**
   * Track share event in analytics
   */
  async trackShare(dogId, platform) {
    try {
      const response = await fetch(`/api/dogs/${encodeURIComponent(dogId)}/share`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          platform: platform,
          timestamp: new Date().toISOString(),
          userAgent: navigator.userAgent,
        }),
      });

      if (!response.ok) {
        console.warn(`SocialShare: Failed to track share - ${response.status}`);
      }
    } catch (error) {
      console.error('SocialShare: Error tracking share', error);
      // Non-blocking - don't throw
    }
  }

  /**
   * Show temporary tooltip notification
   */
  showTooltip(message) {
    // Remove existing tooltip if any
    const existingTooltip = document.querySelector('.social-share-tooltip');
    if (existingTooltip) {
      existingTooltip.remove();
    }

    // Create tooltip element
    const tooltip = document.createElement('div');
    tooltip.className = 'social-share-tooltip';
    tooltip.textContent = message;
    tooltip.style.cssText = `
      position: fixed;
      bottom: 20px;
      left: 50%;
      transform: translateX(-50%);
      background-color: #333;
      color: white;
      padding: 12px 20px;
      border-radius: 4px;
      font-size: 14px;
      z-index: 10000;
      pointer-events: none;
      animation: slideUp 0.3s ease-out;
    `;

    document.body.appendChild(tooltip);

    // Remove tooltip after 2 seconds
    this.tooltipTimeout = setTimeout(() => {
      tooltip.style.animation = 'slideDown 0.3s ease-out';
      setTimeout(() => {
        tooltip.remove();
      }, 300);
    }, 2000);
  }

  /**
   * Escape text for safe display
   */
  escapeText(text) {
    const map = {
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      '"': '&quot;',
      "'": '&#039;',
    };
    return String(text).replace(/[&<>"']/g, (m) => map[m]);
  }

  /**
   * Get singleton instance
   */
  static getInstance() {
    return SocialShare.instance || new SocialShare();
  }
}

// Add CSS animations to document if not already present
function addSocialShareStyles() {
  if (document.querySelector('#social-share-styles')) {
    return;
  }

  const style = document.createElement('style');
  style.id = 'social-share-styles';
  style.textContent = `
    @keyframes slideUp {
      from {
        opacity: 0;
        transform: translateX(-50%) translateY(10px);
      }
      to {
        opacity: 1;
        transform: translateX(-50%) translateY(0);
      }
    }

    @keyframes slideDown {
      from {
        opacity: 1;
        transform: translateX(-50%) translateY(0);
      }
      to {
        opacity: 0;
        transform: translateX(-50%) translateY(10px);
      }
    }
  `;
  document.head.appendChild(style);
}

// Initialize styles when module loads
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', addSocialShareStyles);
} else {
  addSocialShareStyles();
}

// Export to global scope
window.SocialShare = SocialShare;
