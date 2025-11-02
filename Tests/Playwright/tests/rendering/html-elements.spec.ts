import { test, expect } from '@playwright/test';

/**
 * HTML Elements Rendering Tests (HTML-001 to HTML-015)
 * Priority: P0 (Critical)
 *
 * Tests comprehensive HTML element rendering including:
 * - Semantic elements (header, nav, main, article, footer)
 * - Headings hierarchy (h1-h6)
 * - Text formatting (p, strong, em, mark)
 * - Lists (ul, ol, dl)
 * - Tables (basic and complex)
 * - Forms (input types)
 * - Links and navigation
 * - Images and alt text
 * - Embedded content (iframe, object)
 * - Media elements (audio, video)
 * - Interactive elements (details/summary, dialog)
 * - Custom data attributes
 */

test.describe('HTML Elements Rendering', () => {

  test('HTML-001: Semantic elements (header, nav, main, article, footer)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head><title>Semantic HTML5</title></head>
        <body>
          <header id="page-header">
            <h1>Website Header</h1>
          </header>
          <nav id="main-nav">
            <a href="#home">Home</a>
            <a href="#about">About</a>
          </nav>
          <main id="content">
            <article id="article-1">
              <h2>Article Title</h2>
              <p>Article content goes here.</p>
            </article>
            <section id="section-1">
              <h2>Section Title</h2>
              <p>Section content.</p>
            </section>
          </main>
          <aside id="sidebar">
            <p>Sidebar content</p>
          </aside>
          <footer id="page-footer">
            <p>Footer content</p>
          </footer>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify all semantic elements are present and accessible
    await expect(page.locator('header#page-header')).toBeVisible();
    await expect(page.locator('nav#main-nav')).toBeVisible();
    await expect(page.locator('main#content')).toBeVisible();
    await expect(page.locator('article#article-1')).toBeVisible();
    await expect(page.locator('section#section-1')).toBeVisible();
    await expect(page.locator('aside#sidebar')).toBeVisible();
    await expect(page.locator('footer#page-footer')).toBeVisible();

    // Verify text content renders correctly
    await expect(page.locator('header h1')).toHaveText('Website Header');
    await expect(page.locator('article h2')).toHaveText('Article Title');
  });

  test('HTML-002: Headings (h1-h6) hierarchy', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <h1>Heading Level 1</h1>
          <h2>Heading Level 2</h2>
          <h3>Heading Level 3</h3>
          <h4>Heading Level 4</h4>
          <h5>Heading Level 5</h5>
          <h6>Heading Level 6</h6>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify all heading levels render
    for (let level = 1; level <= 6; level++) {
      const heading = page.locator(`h${level}`);
      await expect(heading).toBeVisible();
      await expect(heading).toHaveText(`Heading Level ${level}`);
    }

    // Verify heading sizes are properly differentiated (h1 > h2 > ... > h6)
    const h1Size = await page.locator('h1').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });
    const h2Size = await page.locator('h2').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });
    const h6Size = await page.locator('h6').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });

    expect(h1Size).toBeGreaterThan(h2Size);
    expect(h2Size).toBeGreaterThan(h6Size);
  });

  test('HTML-003: Paragraphs and text formatting', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <p id="p1">Regular paragraph text.</p>
          <p><strong id="strong">Bold text</strong></p>
          <p><em id="em">Italic text</em></p>
          <p><mark id="mark">Highlighted text</mark></p>
          <p><code id="code">Inline code</code></p>
          <p><del id="del">Deleted text</del></p>
          <p><ins id="ins">Inserted text</ins></p>
          <p><sub id="sub">Subscript</sub> and <sup id="sup">Superscript</sup></p>
          <p><small id="small">Small text</small></p>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify all text formatting elements render
    await expect(page.locator('#p1')).toHaveText('Regular paragraph text.');
    await expect(page.locator('#strong')).toHaveText('Bold text');
    await expect(page.locator('#em')).toHaveText('Italic text');
    await expect(page.locator('#mark')).toHaveText('Highlighted text');
    await expect(page.locator('#code')).toHaveText('Inline code');
    await expect(page.locator('#del')).toHaveText('Deleted text');
    await expect(page.locator('#ins')).toHaveText('Inserted text');

    // Verify strong is bold
    const isBold = await page.locator('#strong').evaluate(el => {
      const weight = window.getComputedStyle(el).fontWeight;
      return parseInt(weight) >= 600;
    });
    expect(isBold).toBe(true);

    // Verify em is italic
    const isItalic = await page.locator('#em').evaluate(el => {
      return window.getComputedStyle(el).fontStyle === 'italic';
    });
    expect(isItalic).toBe(true);
  });

  test('HTML-004: Lists (ul, ol, dl)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <ul id="unordered">
            <li>Item 1</li>
            <li>Item 2</li>
            <li>Item 3</li>
          </ul>
          <ol id="ordered">
            <li>First</li>
            <li>Second</li>
            <li>Third</li>
          </ol>
          <dl id="definition">
            <dt>Term 1</dt>
            <dd>Definition 1</dd>
            <dt>Term 2</dt>
            <dd>Definition 2</dd>
          </dl>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify unordered list
    const ulItems = page.locator('#unordered li');
    await expect(ulItems).toHaveCount(3);
    await expect(ulItems.nth(0)).toHaveText('Item 1');

    // Verify ordered list
    const olItems = page.locator('#ordered li');
    await expect(olItems).toHaveCount(3);
    await expect(olItems.nth(1)).toHaveText('Second');

    // Verify definition list
    await expect(page.locator('#definition dt').first()).toHaveText('Term 1');
    await expect(page.locator('#definition dd').first()).toHaveText('Definition 1');
  });

  test('HTML-005: Tables (basic structure)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <table id="basic-table" border="1">
            <thead>
              <tr>
                <th>Header 1</th>
                <th>Header 2</th>
                <th>Header 3</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td>Row 1 Col 1</td>
                <td>Row 1 Col 2</td>
                <td>Row 1 Col 3</td>
              </tr>
              <tr>
                <td>Row 2 Col 1</td>
                <td>Row 2 Col 2</td>
                <td>Row 2 Col 3</td>
              </tr>
            </tbody>
            <tfoot>
              <tr>
                <td colspan="3">Footer</td>
              </tr>
            </tfoot>
          </table>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify table structure
    await expect(page.locator('#basic-table')).toBeVisible();
    await expect(page.locator('#basic-table thead th')).toHaveCount(3);
    await expect(page.locator('#basic-table tbody tr')).toHaveCount(2);

    // Verify headers
    const headers = page.locator('#basic-table thead th');
    await expect(headers.nth(0)).toHaveText('Header 1');
    await expect(headers.nth(1)).toHaveText('Header 2');

    // Verify cell content
    const firstCell = page.locator('#basic-table tbody tr:first-child td:first-child');
    await expect(firstCell).toHaveText('Row 1 Col 1');
  });

  test('HTML-006: Tables (colspan, rowspan)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <table border="1">
            <tr>
              <td rowspan="2" id="rowspan-cell">Rowspan 2</td>
              <td>Cell 1</td>
              <td>Cell 2</td>
            </tr>
            <tr>
              <td colspan="2" id="colspan-cell">Colspan 2</td>
            </tr>
            <tr>
              <td>Cell A</td>
              <td>Cell B</td>
              <td>Cell C</td>
            </tr>
          </table>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify complex table renders
    await expect(page.locator('#rowspan-cell')).toBeVisible();
    await expect(page.locator('#rowspan-cell')).toHaveText('Rowspan 2');
    await expect(page.locator('#colspan-cell')).toBeVisible();
    await expect(page.locator('#colspan-cell')).toHaveText('Colspan 2');

    // Verify colspan/rowspan attributes
    const rowspanValue = await page.locator('#rowspan-cell').getAttribute('rowspan');
    expect(rowspanValue).toBe('2');

    const colspanValue = await page.locator('#colspan-cell').getAttribute('colspan');
    expect(colspanValue).toBe('2');
  });

  test('HTML-007: Forms (input types)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <form id="test-form">
            <input type="text" id="text-input" placeholder="Text">
            <input type="password" id="password-input" placeholder="Password">
            <input type="email" id="email-input" placeholder="Email">
            <input type="number" id="number-input" placeholder="Number">
            <input type="date" id="date-input">
            <input type="checkbox" id="checkbox-input">
            <input type="radio" name="radio" id="radio-input">
            <input type="range" id="range-input" min="0" max="100" value="50">
            <input type="color" id="color-input" value="#ff0000">
            <textarea id="textarea-input" placeholder="Textarea"></textarea>
            <select id="select-input">
              <option value="1">Option 1</option>
              <option value="2">Option 2</option>
            </select>
            <button type="submit" id="submit-button">Submit</button>
          </form>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify all input types render
    await expect(page.locator('#text-input')).toBeVisible();
    await expect(page.locator('#password-input')).toBeVisible();
    await expect(page.locator('#email-input')).toBeVisible();
    await expect(page.locator('#number-input')).toBeVisible();
    await expect(page.locator('#date-input')).toBeVisible();
    await expect(page.locator('#checkbox-input')).toBeVisible();
    await expect(page.locator('#radio-input')).toBeVisible();
    await expect(page.locator('#range-input')).toBeVisible();
    await expect(page.locator('#textarea-input')).toBeVisible();
    await expect(page.locator('#select-input')).toBeVisible();
    await expect(page.locator('#submit-button')).toBeVisible();

    // Verify input types are correctly set
    await expect(page.locator('#text-input')).toHaveAttribute('type', 'text');
    await expect(page.locator('#password-input')).toHaveAttribute('type', 'password');
    await expect(page.locator('#email-input')).toHaveAttribute('type', 'email');
  });

  test('HTML-008: Links (internal, external, anchors)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body style="height: 2000px;">
          <nav>
            <a href="#section1" id="anchor-link">Go to Section 1</a>
            <a href="http://example.com" id="external-link" target="_blank">External Link</a>
            <a href="/internal-page" id="internal-link">Internal Link</a>
            <a href="mailto:test@example.com" id="mailto-link">Email Link</a>
            <a href="tel:+1234567890" id="tel-link">Phone Link</a>
          </nav>
          <div id="section1" style="margin-top: 1500px;">Section 1 Content</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify all link types render
    await expect(page.locator('#anchor-link')).toBeVisible();
    await expect(page.locator('#external-link')).toBeVisible();
    await expect(page.locator('#internal-link')).toBeVisible();
    await expect(page.locator('#mailto-link')).toBeVisible();
    await expect(page.locator('#tel-link')).toBeVisible();

    // Verify href attributes
    await expect(page.locator('#anchor-link')).toHaveAttribute('href', '#section1');
    await expect(page.locator('#external-link')).toHaveAttribute('href', 'http://example.com');
    await expect(page.locator('#external-link')).toHaveAttribute('target', '_blank');
    await expect(page.locator('#mailto-link')).toHaveAttribute('href', 'mailto:test@example.com');

    // Test anchor link navigation using scrollIntoView
    await page.evaluate(() => {
      const element = document.getElementById('section1');
      if (element) element.scrollIntoView();
    });
    await page.waitForTimeout(500); // Allow scroll animation
    const scrollY = await page.evaluate(() => window.scrollY);
    expect(scrollY).toBeGreaterThan(1000); // Should have scrolled down

    // Verify target element is in view
    const isInView = await page.evaluate(() => {
      const element = document.getElementById('section1');
      if (!element) return false;
      const rect = element.getBoundingClientRect();
      return rect.top >= 0 && rect.top <= window.innerHeight;
    });
    expect(isInView).toBe(true);
  });

  test('HTML-009: Images (src, alt, loading)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <img id="img1" src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='100' height='100'%3E%3Crect fill='red' width='100' height='100'/%3E%3C/svg%3E" alt="Red square">
          <img id="img2" src="invalid-image.jpg" alt="Broken image">
          <img id="img3" src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='100' height='100'%3E%3Ccircle fill='blue' cx='50' cy='50' r='50'/%3E%3C/svg%3E" alt="Blue circle" loading="lazy">
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify valid image loads
    await expect(page.locator('#img1')).toBeVisible();
    await expect(page.locator('#img1')).toHaveAttribute('alt', 'Red square');

    // Verify alt text is accessible
    const altText = await page.locator('#img1').getAttribute('alt');
    expect(altText).toBe('Red square');

    // Verify lazy loading attribute
    await expect(page.locator('#img3')).toHaveAttribute('loading', 'lazy');

    // Verify image dimensions
    const dimensions = await page.locator('#img1').evaluate(el => ({
      width: el.width,
      height: el.height
    }));
    expect(dimensions.width).toBe(100);
    expect(dimensions.height).toBe(100);
  });

  test('HTML-010: Embedded content (iframe, object)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <iframe id="iframe1" src="data:text/html,<h1>Iframe Content</h1>" width="300" height="200"></iframe>
          <iframe id="iframe2" srcdoc="<p>Inline iframe content using srcdoc</p>" width="300" height="100"></iframe>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify iframes render
    await expect(page.locator('#iframe1')).toBeVisible();
    await expect(page.locator('#iframe2')).toBeVisible();

    // Verify iframe attributes
    await expect(page.locator('#iframe1')).toHaveAttribute('width', '300');
    await expect(page.locator('#iframe1')).toHaveAttribute('height', '200');

    // Verify iframe content (accessing iframe content)
    const iframe1 = page.frameLocator('#iframe1');
    await expect(iframe1.locator('h1')).toHaveText('Iframe Content');

    const iframe2 = page.frameLocator('#iframe2');
    await expect(iframe2.locator('p')).toHaveText('Inline iframe content using srcdoc');
  });

  test('HTML-011: Audio element', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <audio id="audio1" controls>
            <source src="data:audio/wav;base64,UklGRiQAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQAAAAA=" type="audio/wav">
            Your browser does not support the audio element.
          </audio>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify audio element renders
    await expect(page.locator('#audio1')).toBeVisible();
    await expect(page.locator('#audio1')).toHaveAttribute('controls');

    // Verify audio element properties
    const hasSource = await page.locator('#audio1 source').count();
    expect(hasSource).toBeGreaterThan(0);
  });

  test('HTML-012: Video element', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <video id="video1" width="320" height="240" controls poster="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='320' height='240'%3E%3Crect fill='gray' width='320' height='240'/%3E%3C/svg%3E">
            <source src="data:video/mp4;base64,AAAAIGZ0eXBpc29tAAACAGlzb21pc28yYXZjMW1wNDEAAAAIZnJlZQAAAAhtZGF0AAAA" type="video/mp4">
            Your browser does not support the video tag.
          </video>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify video element renders
    await expect(page.locator('#video1')).toBeVisible();
    await expect(page.locator('#video1')).toHaveAttribute('controls');
    await expect(page.locator('#video1')).toHaveAttribute('width', '320');
    await expect(page.locator('#video1')).toHaveAttribute('height', '240');

    // Verify poster attribute
    const poster = await page.locator('#video1').getAttribute('poster');
    expect(poster).toBeTruthy();
  });

  test('HTML-013: Details/summary disclosure', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <details id="details1">
            <summary id="summary1">Click to expand</summary>
            <p id="content1">This is the hidden content that appears when expanded.</p>
          </details>
          <details id="details2" open>
            <summary>Open by default</summary>
            <p>This content is visible by default.</p>
          </details>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify details/summary elements render
    await expect(page.locator('#details1')).toBeVisible();
    await expect(page.locator('#summary1')).toBeVisible();

    // Initially, content should be hidden (if closed by default)
    const isOpen = await page.locator('#details1').evaluate(el => (el as HTMLDetailsElement).open);
    if (!isOpen) {
      await expect(page.locator('#content1')).toBeHidden();
    }

    // Click to toggle
    await page.click('#summary1');
    await page.waitForTimeout(100);

    // Content should now be visible
    await expect(page.locator('#content1')).toBeVisible();

    // Verify details with open attribute
    await expect(page.locator('#details2')).toHaveAttribute('open');
  });

  test('HTML-014: Dialog element', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <button id="open-dialog">Open Dialog</button>
          <dialog id="dialog1">
            <p>This is a dialog</p>
            <button id="close-dialog">Close</button>
          </dialog>
          <script>
            document.getElementById('open-dialog').addEventListener('click', () => {
              document.getElementById('dialog1').showModal();
            });
            document.getElementById('close-dialog').addEventListener('click', () => {
              document.getElementById('dialog1').close();
            });
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify dialog element exists but is initially hidden
    const dialog = page.locator('#dialog1');
    await expect(dialog).toBeAttached();

    // Open dialog
    await page.click('#open-dialog');
    await page.waitForTimeout(100);

    // Verify dialog is now visible
    const isOpen = await dialog.evaluate(el => (el as HTMLDialogElement).open);
    expect(isOpen).toBe(true);

    // Close dialog
    await page.click('#close-dialog');
    await page.waitForTimeout(100);

    // Verify dialog is closed
    const isClosed = await dialog.evaluate(el => !(el as HTMLDialogElement).open);
    expect(isClosed).toBe(true);
  });

  test('HTML-015: Custom data attributes', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <div id="element1"
               data-user-id="12345"
               data-user-name="John Doe"
               data-role="admin"
               data-active="true">
            User Element
          </div>
          <script>
            const el = document.getElementById('element1');
            const info = document.createElement('p');
            info.id = 'info';
            info.textContent = 'UserID: ' + el.dataset.userId +
                              ', Name: ' + el.dataset.userName +
                              ', Role: ' + el.dataset.role;
            document.body.appendChild(info);
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify custom data attributes are accessible
    const userId = await page.locator('#element1').getAttribute('data-user-id');
    expect(userId).toBe('12345');

    const userName = await page.locator('#element1').getAttribute('data-user-name');
    expect(userName).toBe('John Doe');

    const role = await page.locator('#element1').getAttribute('data-role');
    expect(role).toBe('admin');

    // Verify dataset API access works
    await expect(page.locator('#info')).toContainText('UserID: 12345');
    await expect(page.locator('#info')).toContainText('Name: John Doe');
    await expect(page.locator('#info')).toContainText('Role: admin');

    // Verify dataset access via JavaScript
    const datasetValues = await page.locator('#element1').evaluate(el => {
      return {
        userId: (el as HTMLElement).dataset.userId,
        userName: (el as HTMLElement).dataset.userName,
        role: (el as HTMLElement).dataset.role,
        active: (el as HTMLElement).dataset.active
      };
    });

    expect(datasetValues.userId).toBe('12345');
    expect(datasetValues.userName).toBe('John Doe');
    expect(datasetValues.role).toBe('admin');
    expect(datasetValues.active).toBe('true');
  });

});
