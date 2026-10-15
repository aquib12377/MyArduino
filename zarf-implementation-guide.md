# ZARF — WordPress Theme Implementation Guide

> **For Claude Code.** This is the full brand, design, UX, SEO, and architecture spec. Treat this as the source of truth. Do not deviate from tokens, type, or silk gold treatment without explicit instruction.

---

## 1. Brand Summary

| Field | Value |
|---|---|
| Brand | ZARF |
| Tagline | Luxury Wrapped in Aroma |
| Category | Niche luxury perfume |
| Positioning | Byredo / Amouage / Dior Privée tier — refined, collector-grade |
| Audience | Premium fragrance buyers, gift shoppers, collectors |
| Voice | Minimal, confident, literary. No exclamation marks. No hype words. |
| Vibe | Matte black packaging + metallic silk gold. Restrained luxury. |

---

## 2. Design Tokens

### Colors

```css
:root {
  --zarf-black: #050505;        /* page bg */
  --zarf-charcoal: #111111;     /* section bg */
  --zarf-gold: #C9A24B;         /* primary */
  --zarf-gold-bright: #FFE9A8;  /* highlights, accents */
  --zarf-gold-deep: #5A4218;    /* shadow tone */
  --zarf-bone: #EDE6D6;         /* body text on dark */
  --zarf-mute: #8A8275;         /* secondary text */
  --zarf-line: rgba(201,162,75,0.35);
}
```

### Silk Gold (static — no animation)

```css
:root {
  --silk-gold: linear-gradient(
    105deg,
    #5A4218 0%, #8C6B2A 12%, #C9A24B 25%, #E5C16A 38%,
    #F8DC93 46%, #FFE9A8 50%, #F8DC93 54%, #E5C16A 62%,
    #C9A24B 75%, #8C6B2A 88%, #5A4218 100%
  );
  --silk-gold-vertical: linear-gradient(
    180deg,
    #FFE9A8 0%, #F2D88A 18%, #D4AC56 50%, #A8842F 82%, #6B4F1E 100%
  );
  --silk-edge: linear-gradient(
    135deg,
    #5A4218 0%, #C9A24B 20%, #FFE9A8 35%, #F2D88A 50%,
    #FFE9A8 65%, #C9A24B 80%, #5A4218 100%
  );
}

.silk-text {
  background: var(--silk-gold);
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent;
  filter:
    drop-shadow(0 1px 0 #1a1208)
    drop-shadow(0 0 1px rgba(0,0,0,0.9))
    drop-shadow(0 0 12px rgba(201,162,75,0.25));
}
```

**Apply `.silk-text` to:** wordmark, H1, product names, active nav, price labels.

### Typography

```css
/* Google Fonts */
@import url('https://fonts.googleapis.com/css2?family=Cormorant+Garamond:ital,wght@0,300;0,400;0,500;0,600;1,400&family=Jost:wght@300;400;500;600&display=swap');
```

| Use | Font | Size | Tracking |
|---|---|---|---|
| Display H1 | Cormorant Garamond 400 | 64–72px | 0.02em |
| H2 | Cormorant Garamond 400 | 42–48px | 0.02em |
| H3 | Cormorant Garamond 500 | 22–28px | 0.1em uppercase |
| Body | Jost 300 | 15–16px | normal, line-height 1.8 |
| Caption / Nav | Jost 400 uppercase | 11–12px | 0.25–0.4em |
| Price | Cormorant Garamond 500 | 18px | 0.1em |

### Spacing Scale

`4 / 8 / 16 / 24 / 40 / 64 / 96 / 128 / 160 px`. Use as multiples only.

### Shadows

```css
--shadow-card:        0 20px 60px rgba(0,0,0,0.45);
--shadow-card-hover:  0 30px 80px rgba(0,0,0,0.6), 0 0 40px rgba(201,162,75,0.12);
--shadow-button:      inset 0 1px 0 rgba(255,255,230,0.9),
                      inset 0 -2px 4px rgba(60,40,10,0.6),
                      0 0 0 1px #8C6B2A,
                      0 6px 20px rgba(0,0,0,0.5);
--shadow-button-press: inset 0 3px 6px rgba(60,40,10,0.7),
                       inset 0 -1px 0 rgba(255,233,168,0.3);
```

### Motion

- All transitions: `300–500ms ease-out`. No bounce, no elastic.
- Scroll-in: fade + 8px translateY.
- Hover on cards: translateY(-4px) + intensify border glow.
- **No shimmer, no autoplay carousels, no parallax hero.**

---

## 3. UX Principles

1. **Mobile-first.** Design at 375px, scale up. All touch targets ≥ 44px.
2. **Whitespace > density.** Luxury = breathing room. Min section padding 64px desktop / 32px mobile.
3. **One CTA per section.** Never compete for attention.
4. **Cart drawer, not page.** Stay in context.
5. **Core Web Vitals green.** LCP < 2.5s, CLS < 0.1, INP < 200ms.
6. **Max 2 web fonts loaded.** Subset to Latin only.
7. **Images: WebP/AVIF, lazy-loaded, width/height attrs set** to prevent CLS.
8. **Accessibility:** WCAG AA contrast, ARIA labels on icon buttons, focus-visible rings in gold.

---

## 4. UI Components (spec)

### Buttons

```html
<a class="btn btn-primary">Discover Now</a>  <!-- silk gold fill, dark text -->
<a class="btn btn-ghost">View Collection</a>  <!-- 1px gold border, transparent -->
```

- Primary: `var(--silk-gold-vertical)`, dark text `#1a1208`, `text-transform: uppercase`, `letter-spacing: 0.4em`, padding `18px 42px`.
- Hover: `translateY(-2px)` + brighter shadow.
- Active: letterpress (shadow stack above).
- Ghost: `1px solid var(--zarf-gold)`, fills gold on hover.

### Product Card

- Aspect ratio 3:4. Dark background with `inset 0 0 60px rgba(0,0,0,0.5)`.
- 1px silk-edge border.
- Product name in Cormorant, price in silk-text below.
- Hover: lift 4px, border brightens to `--zarf-gold-bright`.

### Hairline Divider

```css
.divider {
  height: 1px;
  background: linear-gradient(90deg,
    transparent 0%, var(--zarf-line) 20%,
    #FFE9A8 50%, var(--zarf-line) 80%, transparent 100%);
}
```

### Form Inputs (waitlist, contact)

- Bare bottom border in `--zarf-line`, transparent fill.
- Focus: border becomes `--zarf-gold-bright` + faint glow.
- Label floats above on focus.

### Page Grain Overlay (global)

```css
body::before {
  content: ""; position: fixed; inset: 0; pointer-events: none;
  z-index: 9999; opacity: 0.05;
  background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='200' height='200'><filter id='n'><feTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='2' stitchTiles='stitch'/></filter><rect width='100%25' height='100%25' filter='url(%23n)'/></svg>");
}
```

---

## 5. Pages & Templates

| Route | Template | Purpose |
|---|---|---|
| `/` | `front-page.php` | Hero, collection, story, waitlist |
| `/collection/` | `archive-perfume.php` | Grid of all perfumes |
| `/collection/{slug}/` | `single-perfume.php` | Product detail + schema |
| `/ingredients/` | `taxonomy-notes.php` | Notes library (Oud, Woody, etc.) |
| `/ingredients/{note}/` | `taxonomy-notes.php` | Perfumes by note |
| `/our-story/` | `page-our-story.php` | Brand narrative |
| `/journal/` | `home.php` | Blog index |
| `/journal/{slug}/` | `single.php` | Article |
| `/contact/` | `page-contact.php` | Contact + form |
| `/cart/`, `/checkout/` | Woo defaults | Themed |

### Homepage sections (in order)

1. Sticky nav (transparent → solid on scroll)
2. Hero: split layout, copy left, bottle right, single CTA
3. Trust strip 1: Long-Lasting · Natural Ingredients · Elegant Packaging
4. Trust strip 2: Fast Shipping · Secure Payment · Easy Returns
5. Editorial block: "Discover Our Collection" (split image/text)
6. Product grid: 3 featured (Woody A · Signature Gift Box · Woody A Set)
7. Story teaser → links to `/our-story/`
8. Waitlist capture
9. Footer (4 cols + socials + hairline gold rule)

### Product detail layout

- Above fold: product image left, info right (name, tagline, price, notes pyramid, CTA).
- Below: full description, ingredients accordion, FAQ (schema), related perfumes.

---

## 6. WordPress Architecture

### Stack

- WordPress 6.x classic theme (not FSE/block theme — keep control)
- WooCommerce for commerce
- ACF Pro for custom fields
- Tailwind via `@wordpress/scripts` OR vanilla CSS (pick one, no mix)
- No page builders. No Elementor.

### Theme file structure

```
zarf/
├── style.css                 # theme header only
├── functions.php
├── index.php
├── front-page.php
├── header.php
├── footer.php
├── single-perfume.php        # CPT single
├── archive-perfume.php
├── taxonomy-notes.php
├── single.php                # blog
├── home.php                  # blog index
├── page-our-story.php
├── page-contact.php
├── 404.php
├── searchform.php
├── /inc/
│   ├── cpt-perfume.php       # register CPT + taxonomies
│   ├── acf-fields.php        # field groups
│   ├── schema.php            # JSON-LD generators
│   ├── enqueue.php           # scripts/styles
│   ├── theme-support.php
│   ├── waitlist.php          # AJAX handler + storage
│   └── woocommerce.php       # Woo overrides
├── /template-parts/
│   ├── nav.php
│   ├── hero.php
│   ├── trust-strip.php
│   ├── product-card.php
│   ├── notes-pyramid.php
│   ├── waitlist-form.php
│   └── footer-cols.php
├── /assets/
│   ├── /css/
│   │   ├── main.css
│   │   └── silk.css          # silk gold utilities
│   ├── /js/
│   │   └── main.js
│   └── /images/
└── README.md
```

### Custom Post Type: `perfume`

```php
// inc/cpt-perfume.php
register_post_type('perfume', [
  'label' => 'Perfumes',
  'public' => true,
  'has_archive' => 'collection',
  'rewrite' => ['slug' => 'collection'],
  'supports' => ['title','editor','thumbnail','excerpt'],
  'show_in_rest' => true,
  'menu_icon' => 'dashicons-beer',
]);
```

### Taxonomies

- `notes` (non-hierarchical): Oud, Woody, Floral, Amber, Citrus, Musk, Spice, Leather
- `category_perfume` (hierarchical): Unisex, Female, Gift Sets

### ACF Fields for `perfume`

| Field | Type | Required |
|---|---|---|
| short_description | Text (160 char) | yes |
| top_notes | Text | yes |
| heart_notes | Text | yes |
| base_notes | Text | yes |
| mood | Text | no |
| concentration | Select (EDP, EDT, Parfum, Extrait) | yes |
| volume_ml | Number | yes |
| bottle_gallery | Gallery (3–6 images) | yes |
| featured_flag | True/False | no |
| sku | Text | no |
| faq_items | Repeater (question, answer) | no |
| pairs_well_with | Post Object (perfume, multi) | no |

### Woo integration

- Map each `perfume` CPT to a Woo product via ACF `product_link` or use Woo products directly and add meta via ACF. **Recommended:** use Woo products as primary, treat `perfume` CPT as editorial record that links to Woo product for checkout. Alternatively, skip the CPT and use Woo product categories `unisex`, `female` with ACF on Woo products.
- **Decision needed from user:** CPT + linked Woo product, OR Woo products only with extended ACF? Default to Woo products + ACF unless told otherwise.

---

## 7. SEO

### URL structure

```
/                           # homepage
/collection/                # archive
/collection/woody-a/        # product
/ingredients/oud/           # note taxonomy
/journal/                   # blog
/journal/art-of-oud/        # post
/our-story/
/contact/
```

- No dates in blog URLs. No `/blog/` prefix — use `/journal/`.
- Canonical tags on every page via `wp_head`.
- Trailing slashes enforced.

### JSON-LD Schemas (inject in `inc/schema.php`)

**Organization (every page, in header.php)**

```json
{
  "@context":"https://schema.org",
  "@type":"Organization",
  "name":"ZARF",
  "url":"https://zarf.com",
  "logo":"https://zarf.com/logo.png",
  "sameAs":["https://instagram.com/zarf","https://tiktok.com/@zarf"]
}
```

**Product (single-perfume.php)**

```json
{
  "@context":"https://schema.org",
  "@type":"Product",
  "name":"Woody A",
  "brand":{"@type":"Brand","name":"ZARF"},
  "description":"...",
  "image":["..."],
  "sku":"...",
  "offers":{
    "@type":"Offer",
    "price":"89.00",
    "priceCurrency":"USD",
    "availability":"https://schema.org/InStock"
  },
  "aggregateRating":{...}  // if reviews exist
}
```

**BreadcrumbList** — on all non-home pages.

**FAQPage** — on product pages where `faq_items` repeater has rows.

**Article** — on `single.php` (blog).

### Meta tags (header.php)

- `<title>` via `wp_title` or Yoast
- Meta description from `short_description` (product) or excerpt (blog)
- OG: `og:title`, `og:description`, `og:image` (1200×630), `og:type` (`website` / `product` / `article`)
- Twitter: `summary_large_image`
- `robots`: index,follow on all public pages; noindex on `/cart/`, `/checkout/`, `/account/`

### Internal linking strategy

- Every product page links to 2 `notes` taxonomy pages and 1 journal article that mentions it.
- Every journal article links to 1–2 products and the note pages it covers.
- Homepage links to top 3 products + top 3 notes.

### AI/LLM readiness

- Semantic HTML5: `<article>`, `<section>`, `<nav>`, `<aside>`, `<main>`.
- H1 → H2 → H3 hierarchy, no skips.
- FAQ block per product for extraction.
- Entity-rich product copy: brand → product → notes → mood → occasion.

### Performance

- `wp_enqueue_script` with `defer`.
- Preload Cormorant + Jost WOFF2 subsets.
- Use `fetchpriority="high"` on hero image.
- Lazy-load below-fold images (`loading="lazy"`).
- Critical CSS inlined in `<head>`, rest deferred.
- No jQuery dependency. Vanilla JS only.

---

## 8. Functional Requirements

### Waitlist

- Email-only form, stored in `wp_options` array or custom table `wp_zarf_waitlist`.
- AJAX submission via `admin-ajax.php` or REST API `/wp-json/zarf/v1/waitlist`.
- Honeypot field + nonce for spam protection.
- Success state: inline confirmation (no page reload).
- Export to CSV from admin.

### Search

- Native WP search scoped to `perfume` CPT + `post`.
- Search form in nav opens drawer overlay.

### Language

- EN-first. Arabic (RTL) support planned — use WPML or Polylang. Design all components to flip via `[dir="rtl"]`.

---

## 9. Implementation Order for Claude Code

1. Scaffold theme folder + `style.css` header + `functions.php` with `after_setup_theme`.
2. Enqueue Google Fonts + compile Tailwind or base CSS.
3. Build `header.php` + `footer.php` + nav template part with silk wordmark.
4. Build `front-page.php` with hero, trust, collection, story, waitlist sections.
5. Register `perfume` CPT + `notes` and `category_perfume` taxonomies.
6. Register ACF field groups (JSON-exported into `/acf-json/`).
7. Build `single-perfume.php` with notes pyramid + FAQ.
8. Build `archive-perfume.php` + `taxonomy-notes.php`.
9. Implement `inc/schema.php` — inject JSON-LD in `wp_head`.
10. Wire WooCommerce — theme Woo templates, cart drawer.
11. Build waitlist endpoint + admin export.
12. Blog templates (`home.php`, `single.php`).
13. Contact page + form.
14. 404 + search.
15. Performance pass: critical CSS, preload, lazy-load.
16. SEO pass: sitemap via Yoast/RankMath, `robots.txt`, canonicals.
17. Accessibility audit: focus rings, ARIA, contrast.
18. Mobile QA at 375 / 768 / 1024 / 1440px.

---

## 10. Hard Rules for Claude Code

- Never use purple gradients or generic AI aesthetics.
- Never replace Cormorant Garamond or Jost without approval.
- Never animate the silk gradient (shimmer removed by design).
- Never use `Inter` or system fonts for body.
- Never use a page builder.
- Never inline styles in PHP templates — use CSS classes.
- Every image tag needs `width`, `height`, `alt`, `loading` attributes.
- Every form needs a nonce and honeypot.
- Every external link needs `rel="noopener"` if `target="_blank"`.
- Commit the design tokens as CSS variables in one place only: `assets/css/main.css`.

---

## 11. Open Decisions (confirm before coding)

1. **CPT strategy**: custom `perfume` CPT + linked Woo product, OR Woo products only with ACF? (default: Woo + ACF)
2. **CSS approach**: Tailwind via `@wordpress/scripts` OR vanilla CSS with custom properties? (default: vanilla CSS — lighter, fewer deps)
3. **RTL / Arabic**: ship EN only first, add WPML/Polylang in phase 2? (default: yes, phase 2)
4. **Reviews**: native Woo reviews, or third-party (Judge.me, Stamped)? (default: native Woo)
5. **Payment**: Stripe + Apple Pay? PayPal? COD? (default: Stripe + Apple Pay + COD for MENA)

Answer these four before starting Phase 3 code.
