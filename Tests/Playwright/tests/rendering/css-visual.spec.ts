import { test, expect } from '@playwright/test';

/**
 * CSS Visual Styling Tests (VIS-001 to VIS-015)
 * Priority: P0 (Critical)
 *
 * Tests comprehensive CSS visual styling including:
 * - Colors (hex, rgb, hsl, named)
 * - Backgrounds (color, image, gradient)
 * - Borders (width, style, color, radius)
 * - Shadows (box-shadow, text-shadow)
 * - Opacity and transparency
 * - Transforms (translate, rotate, scale)
 * - Transitions
 * - Animations (keyframes)
 * - Filters (blur, grayscale, etc.)
 * - Blend modes
 * - Fonts (family, size, weight, style)
 * - Text properties (align, decoration, transform)
 * - Line height and letter spacing
 * - Pseudo-elements (::before, ::after)
 * - Pseudo-classes (:hover, :focus, :active)
 */

test.describe('CSS Visual Styling', () => {

  test('VIS-001: Colors (hex, rgb, hsl, named)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .hex { color: #FF5722; }
            .rgb { color: rgb(76, 175, 80); }
            .rgba { color: rgba(33, 150, 243, 0.7); }
            .hsl { color: hsl(300, 76%, 72%); }
            .hsla { color: hsla(60, 100%, 50%, 0.8); }
            .named { color: rebeccapurple; }
          </style>
        </head>
        <body>
          <p class="hex" id="hex">Hex color</p>
          <p class="rgb" id="rgb">RGB color</p>
          <p class="rgba" id="rgba">RGBA color</p>
          <p class="hsl" id="hsl">HSL color</p>
          <p class="hsla" id="hsla">HSLA color</p>
          <p class="named" id="named">Named color</p>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify all color formats work
    const hexColor = await page.locator('#hex').evaluate(el => {
      return window.getComputedStyle(el).color;
    });
    expect(hexColor).toBeTruthy();

    const rgbColor = await page.locator('#rgb').evaluate(el => {
      return window.getComputedStyle(el).color;
    });
    expect(rgbColor).toContain('76'); // Contains RGB value

    const namedColor = await page.locator('#named').evaluate(el => {
      return window.getComputedStyle(el).color;
    });
    expect(namedColor).toBeTruthy(); // rebeccapurple

    // Verify all elements are visible
    await expect(page.locator('#hex')).toBeVisible();
    await expect(page.locator('#rgb')).toBeVisible();
    await expect(page.locator('#named')).toBeVisible();
  });

  test('VIS-002: Backgrounds (color, image, gradient)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .bg-box {
              width: 200px;
              height: 100px;
              margin: 10px;
              padding: 20px;
            }
            #bg-color {
              background-color: #3F51B5;
            }
            #bg-image {
              background-image: url('data:image/svg+xml,%3Csvg xmlns="http://www.w3.org/2000/svg" width="100" height="100"%3E%3Crect fill="red" width="100" height="100"/%3E%3C/svg%3E');
              background-size: cover;
            }
            #bg-linear {
              background: linear-gradient(to right, #FF5722, #FFC107);
            }
            #bg-radial {
              background: radial-gradient(circle, #4CAF50, #009688);
            }
          </style>
        </head>
        <body>
          <div class="bg-box" id="bg-color">Solid Color</div>
          <div class="bg-box" id="bg-image">Background Image</div>
          <div class="bg-box" id="bg-linear">Linear Gradient</div>
          <div class="bg-box" id="bg-radial">Radial Gradient</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify background color
    const bgColor = await page.locator('#bg-color').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(bgColor).toContain('63, 81, 181'); // #3F51B5

    // Verify background image
    const bgImage = await page.locator('#bg-image').evaluate(el => {
      return window.getComputedStyle(el).backgroundImage;
    });
    expect(bgImage).toContain('url');

    // Verify gradients
    const linearGradient = await page.locator('#bg-linear').evaluate(el => {
      return window.getComputedStyle(el).backgroundImage;
    });
    expect(linearGradient).toContain('gradient');

    const radialGradient = await page.locator('#bg-radial').evaluate(el => {
      return window.getComputedStyle(el).backgroundImage;
    });
    expect(radialGradient).toContain('gradient');
  });

  test('VIS-003: Borders (width, style, color, radius)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .border-box {
              width: 150px;
              height: 80px;
              margin: 10px;
              padding: 10px;
              display: inline-block;
            }
            #solid { border: 3px solid #000; }
            #dashed { border: 3px dashed #FF5722; }
            #dotted { border: 3px dotted #4CAF50; }
            #double { border: 5px double #2196F3; }
            #rounded { border: 3px solid #000; border-radius: 15px; }
            #circle {
              width: 100px;
              height: 100px;
              border: 3px solid #9C27B0;
              border-radius: 50%;
            }
          </style>
        </head>
        <body>
          <div class="border-box" id="solid">Solid</div>
          <div class="border-box" id="dashed">Dashed</div>
          <div class="border-box" id="dotted">Dotted</div>
          <div class="border-box" id="double">Double</div>
          <div class="border-box" id="rounded">Rounded</div>
          <div class="border-box" id="circle">Circle</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify border styles
    const solidBorder = await page.locator('#solid').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        width: style.borderTopWidth,
        style: style.borderTopStyle,
        color: style.borderTopColor
      };
    });
    expect(solidBorder.width).toBe('3px');
    expect(solidBorder.style).toBe('solid');

    const dashedBorder = await page.locator('#dashed').evaluate(el => {
      return window.getComputedStyle(el).borderTopStyle;
    });
    expect(dashedBorder).toBe('dashed');

    // Verify border-radius
    const roundedRadius = await page.locator('#rounded').evaluate(el => {
      return window.getComputedStyle(el).borderTopLeftRadius;
    });
    expect(roundedRadius).toBe('15px');

    const circleRadius = await page.locator('#circle').evaluate(el => {
      return window.getComputedStyle(el).borderTopLeftRadius;
    });
    expect(circleRadius).toMatch(/50(px|%)|100px/); // Accept '50px', '50%', or normalized '100px'
  });

  test('VIS-004: Shadows (box-shadow, text-shadow)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .shadow-box {
              width: 200px;
              height: 100px;
              margin: 30px;
              padding: 20px;
              background-color: #fff;
            }
            #box-shadow {
              box-shadow: 5px 5px 10px rgba(0, 0, 0, 0.5);
            }
            #inset-shadow {
              box-shadow: inset 0 0 10px rgba(0, 0, 0, 0.5);
            }
            #multiple-shadows {
              box-shadow: 3px 3px 5px red, -3px -3px 5px blue;
            }
            .text-shadow {
              font-size: 24px;
              font-weight: bold;
            }
            #text-shadow {
              text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5);
            }
          </style>
        </head>
        <body>
          <div class="shadow-box" id="box-shadow">Box Shadow</div>
          <div class="shadow-box" id="inset-shadow">Inset Shadow</div>
          <div class="shadow-box" id="multiple-shadows">Multiple Shadows</div>
          <p class="text-shadow" id="text-shadow">Text Shadow</p>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify box-shadow
    const boxShadow = await page.locator('#box-shadow').evaluate(el => {
      return window.getComputedStyle(el).boxShadow;
    });
    expect(boxShadow).toBeTruthy();
    expect(boxShadow).not.toBe('none');

    // Verify inset shadow
    const insetShadow = await page.locator('#inset-shadow').evaluate(el => {
      return window.getComputedStyle(el).boxShadow;
    });
    expect(insetShadow).toContain('inset');

    // Verify text-shadow
    const textShadow = await page.locator('#text-shadow').evaluate(el => {
      return window.getComputedStyle(el).textShadow;
    });
    expect(textShadow).toBeTruthy();
    expect(textShadow).not.toBe('none');
  });

  test('VIS-005: Opacity and transparency', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .opacity-box {
              width: 150px;
              height: 100px;
              background-color: #2196F3;
              margin: 10px;
              display: inline-block;
            }
            #opacity-1 { opacity: 1; }
            #opacity-0-5 { opacity: 0.5; }
            #opacity-0-2 { opacity: 0.2; }
            #transparent-bg {
              background-color: rgba(244, 67, 54, 0.5);
            }
          </style>
        </head>
        <body>
          <div class="opacity-box" id="opacity-1">100%</div>
          <div class="opacity-box" id="opacity-0-5">50%</div>
          <div class="opacity-box" id="opacity-0-2">20%</div>
          <div class="opacity-box" id="transparent-bg">RGBA</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify opacity values
    const opacity1 = await page.locator('#opacity-1').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).opacity);
    });
    expect(opacity1).toBe(1);

    const opacity05 = await page.locator('#opacity-0-5').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).opacity);
    });
    expect(opacity05).toBe(0.5);

    const opacity02 = await page.locator('#opacity-0-2').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).opacity);
    });
    expect(opacity02).toBe(0.2);

    // Verify RGBA transparency
    const rgbaBg = await page.locator('#transparent-bg').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(rgbaBg).toContain('0.5'); // alpha value
  });

  test('VIS-006: Transforms (translate, rotate, scale)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .transform-box {
              width: 100px;
              height: 100px;
              background-color: #FF9800;
              margin: 50px;
              display: inline-block;
            }
            #translate { transform: translate(20px, 30px); }
            #rotate { transform: rotate(45deg); }
            #scale { transform: scale(1.5); }
            #skew { transform: skew(20deg, 10deg); }
            #multiple { transform: translate(10px, 10px) rotate(30deg) scale(0.8); }
          </style>
        </head>
        <body>
          <div class="transform-box" id="translate">Translate</div>
          <div class="transform-box" id="rotate">Rotate</div>
          <div class="transform-box" id="scale">Scale</div>
          <div class="transform-box" id="skew">Skew</div>
          <div class="transform-box" id="multiple">Multiple</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify transforms
    const translate = await page.locator('#translate').evaluate(el => {
      return window.getComputedStyle(el).transform;
    });
    expect(translate).toBeTruthy();
    expect(translate).not.toBe('none');

    const rotate = await page.locator('#rotate').evaluate(el => {
      return window.getComputedStyle(el).transform;
    });
    expect(rotate).toBeTruthy();
    expect(rotate).not.toBe('none');

    const scale = await page.locator('#scale').evaluate(el => {
      return window.getComputedStyle(el).transform;
    });
    expect(scale).toBeTruthy();
    expect(scale).not.toBe('none');

    // Verify elements are still visible after transform
    await expect(page.locator('#translate')).toBeVisible();
    await expect(page.locator('#rotate')).toBeVisible();
    await expect(page.locator('#scale')).toBeVisible();
  });

  test('VIS-007: Transitions', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .transition-box {
              width: 100px;
              height: 100px;
              background-color: #4CAF50;
              transition: all 0.3s ease;
              margin: 20px;
            }
            .transition-box:hover {
              width: 200px;
              background-color: #FF5722;
            }
            #specific-transition {
              transition-property: background-color;
              transition-duration: 0.5s;
              transition-timing-function: ease-in-out;
              transition-delay: 0.1s;
            }
          </style>
        </head>
        <body>
          <div class="transition-box" id="transition1">Hover me</div>
          <div class="transition-box" id="specific-transition">Specific</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify transition properties
    const transition = await page.locator('#transition1').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        property: style.transitionProperty,
        duration: style.transitionDuration,
        timingFunction: style.transitionTimingFunction
      };
    });
    expect(transition.property).toBe('all');
    expect(transition.duration).toBe('0.3s');

    const specificTransition = await page.locator('#specific-transition').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        property: style.transitionProperty,
        duration: style.transitionDuration,
        delay: style.transitionDelay
      };
    });
    expect(specificTransition.property).toBe('background-color');
    expect(specificTransition.duration).toBe('0.5s');
    expect(specificTransition.delay).toBe('0.1s');
  });

  test('VIS-008: Animations (keyframes)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            @keyframes slideIn {
              from {
                transform: translateX(-100px);
                opacity: 0;
              }
              to {
                transform: translateX(0);
                opacity: 1;
              }
            }
            @keyframes pulse {
              0%, 100% { transform: scale(1); }
              50% { transform: scale(1.2); }
            }
            .animated {
              width: 100px;
              height: 100px;
              background-color: #9C27B0;
              margin: 20px;
            }
            #slide {
              animation: slideIn 1s ease-out;
            }
            #pulse {
              animation: pulse 2s infinite;
            }
            #detailed {
              animation-name: slideIn;
              animation-duration: 1.5s;
              animation-timing-function: ease-in;
              animation-delay: 0.5s;
              animation-iteration-count: 3;
              animation-direction: alternate;
            }
          </style>
        </head>
        <body>
          <div class="animated" id="slide">Slide In</div>
          <div class="animated" id="pulse">Pulse</div>
          <div class="animated" id="detailed">Detailed</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify animation properties
    const slideAnimation = await page.locator('#slide').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        name: style.animationName,
        duration: style.animationDuration
      };
    });
    expect(slideAnimation.name).toBe('slideIn');
    expect(slideAnimation.duration).toBe('1s');

    const pulseAnimation = await page.locator('#pulse').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        name: style.animationName,
        iterationCount: style.animationIterationCount
      };
    });
    expect(pulseAnimation.name).toBe('pulse');
    expect(pulseAnimation.iterationCount).toBe('infinite');

    const detailedAnimation = await page.locator('#detailed').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        name: style.animationName,
        duration: style.animationDuration,
        delay: style.animationDelay,
        iterationCount: style.animationIterationCount,
        direction: style.animationDirection
      };
    });
    expect(detailedAnimation.name).toBe('slideIn');
    expect(detailedAnimation.duration).toBe('1.5s');
    expect(detailedAnimation.delay).toBe('0.5s');
    expect(detailedAnimation.iterationCount).toBe('3');
    expect(detailedAnimation.direction).toBe('alternate');
  });

  test('VIS-009: Filters (blur, grayscale, etc.)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .filter-box {
              width: 150px;
              height: 100px;
              background-color: #FF5722;
              margin: 10px;
              display: inline-block;
            }
            #blur { filter: blur(5px); }
            #grayscale { filter: grayscale(100%); }
            #brightness { filter: brightness(150%); }
            #contrast { filter: contrast(200%); }
            #sepia { filter: sepia(100%); }
            #hue-rotate { filter: hue-rotate(90deg); }
            #saturate { filter: saturate(200%); }
            #multiple { filter: blur(2px) brightness(120%) contrast(150%); }
          </style>
        </head>
        <body>
          <div class="filter-box" id="blur">Blur</div>
          <div class="filter-box" id="grayscale">Grayscale</div>
          <div class="filter-box" id="brightness">Brightness</div>
          <div class="filter-box" id="contrast">Contrast</div>
          <div class="filter-box" id="sepia">Sepia</div>
          <div class="filter-box" id="hue-rotate">Hue Rotate</div>
          <div class="filter-box" id="saturate">Saturate</div>
          <div class="filter-box" id="multiple">Multiple</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify filter properties
    const blur = await page.locator('#blur').evaluate(el => {
      return window.getComputedStyle(el).filter;
    });
    expect(blur).toContain('blur');

    const grayscale = await page.locator('#grayscale').evaluate(el => {
      return window.getComputedStyle(el).filter;
    });
    expect(grayscale).toContain('grayscale');

    const brightness = await page.locator('#brightness').evaluate(el => {
      return window.getComputedStyle(el).filter;
    });
    expect(brightness).toContain('brightness');

    const multiple = await page.locator('#multiple').evaluate(el => {
      return window.getComputedStyle(el).filter;
    });
    expect(multiple).toContain('blur');
    expect(multiple).toContain('brightness');
    expect(multiple).toContain('contrast');
  });

  test('VIS-010: Blend modes', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .blend-container {
              position: relative;
              width: 200px;
              height: 200px;
              background-color: #FF5722;
            }
            .blend-box {
              position: absolute;
              width: 150px;
              height: 150px;
              background-color: #2196F3;
            }
            #multiply { mix-blend-mode: multiply; }
            #screen { mix-blend-mode: screen; }
            #overlay { mix-blend-mode: overlay; }
            #difference { mix-blend-mode: difference; }
          </style>
        </head>
        <body>
          <div class="blend-container">
            <div class="blend-box" id="multiply">Multiply</div>
          </div>
          <div class="blend-container">
            <div class="blend-box" id="screen">Screen</div>
          </div>
          <div class="blend-container">
            <div class="blend-box" id="overlay">Overlay</div>
          </div>
          <div class="blend-container">
            <div class="blend-box" id="difference">Difference</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify blend modes
    const multiply = await page.locator('#multiply').evaluate(el => {
      return window.getComputedStyle(el).mixBlendMode;
    });
    expect(multiply).toBe('multiply');

    const screen = await page.locator('#screen').evaluate(el => {
      return window.getComputedStyle(el).mixBlendMode;
    });
    expect(screen).toBe('screen');

    const overlay = await page.locator('#overlay').evaluate(el => {
      return window.getComputedStyle(el).mixBlendMode;
    });
    expect(overlay).toBe('overlay');

    const difference = await page.locator('#difference').evaluate(el => {
      return window.getComputedStyle(el).mixBlendMode;
    });
    expect(difference).toBe('difference');
  });

  test('VIS-011: Fonts (family, size, weight, style)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            #serif { font-family: serif; }
            #sans-serif { font-family: sans-serif; }
            #monospace { font-family: monospace; }
            #size-12 { font-size: 12px; }
            #size-24 { font-size: 24px; }
            #size-36 { font-size: 36px; }
            #weight-normal { font-weight: normal; }
            #weight-bold { font-weight: bold; }
            #weight-700 { font-weight: 700; }
            #style-normal { font-style: normal; }
            #style-italic { font-style: italic; }
            #style-oblique { font-style: oblique; }
          </style>
        </head>
        <body>
          <p id="serif">Serif font</p>
          <p id="sans-serif">Sans-serif font</p>
          <p id="monospace">Monospace font</p>
          <p id="size-12">12px size</p>
          <p id="size-24">24px size</p>
          <p id="size-36">36px size</p>
          <p id="weight-normal">Normal weight</p>
          <p id="weight-bold">Bold weight</p>
          <p id="weight-700">700 weight</p>
          <p id="style-normal">Normal style</p>
          <p id="style-italic">Italic style</p>
          <p id="style-oblique">Oblique style</p>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify font families
    const serif = await page.locator('#serif').evaluate(el => {
      return window.getComputedStyle(el).fontFamily;
    });
    expect(serif).toContain('serif');

    // Verify font sizes
    const size12 = await page.locator('#size-12').evaluate(el => {
      return window.getComputedStyle(el).fontSize;
    });
    expect(size12).toBe('12px');

    const size24 = await page.locator('#size-24').evaluate(el => {
      return window.getComputedStyle(el).fontSize;
    });
    expect(size24).toBe('24px');

    // Verify font weights
    const weightBold = await page.locator('#weight-bold').evaluate(el => {
      return parseInt(window.getComputedStyle(el).fontWeight);
    });
    expect(weightBold).toBeGreaterThanOrEqual(600);

    // Verify font styles
    const styleItalic = await page.locator('#style-italic').evaluate(el => {
      return window.getComputedStyle(el).fontStyle;
    });
    expect(styleItalic).toBe('italic');
  });

  test('VIS-012: Text properties (align, decoration, transform)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .text-box { width: 300px; padding: 10px; border: 1px solid #ccc; margin: 10px 0; }
            #align-left { text-align: left; }
            #align-center { text-align: center; }
            #align-right { text-align: right; }
            #align-justify { text-align: justify; }
            #underline { text-decoration: underline; }
            #overline { text-decoration: overline; }
            #line-through { text-decoration: line-through; }
            #uppercase { text-transform: uppercase; }
            #lowercase { text-transform: lowercase; }
            #capitalize { text-transform: capitalize; }
          </style>
        </head>
        <body>
          <div class="text-box" id="align-left">Left aligned text</div>
          <div class="text-box" id="align-center">Center aligned text</div>
          <div class="text-box" id="align-right">Right aligned text</div>
          <div class="text-box" id="align-justify">Justified text with multiple words to demonstrate justify alignment</div>
          <p id="underline">Underlined text</p>
          <p id="overline">Overlined text</p>
          <p id="line-through">Strike-through text</p>
          <p id="uppercase">uppercase text</p>
          <p id="lowercase">LOWERCASE TEXT</p>
          <p id="capitalize">capitalize this text</p>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify text alignment
    const alignCenter = await page.locator('#align-center').evaluate(el => {
      return window.getComputedStyle(el).textAlign;
    });
    expect(alignCenter).toBe('center');

    const alignRight = await page.locator('#align-right').evaluate(el => {
      return window.getComputedStyle(el).textAlign;
    });
    expect(alignRight).toBe('right');

    // Verify text decoration
    const underline = await page.locator('#underline').evaluate(el => {
      return window.getComputedStyle(el).textDecoration;
    });
    expect(underline).toContain('underline');

    const lineThrough = await page.locator('#line-through').evaluate(el => {
      return window.getComputedStyle(el).textDecoration;
    });
    expect(lineThrough).toContain('line-through');

    // Verify text transform
    const uppercase = await page.locator('#uppercase').evaluate(el => {
      return window.getComputedStyle(el).textTransform;
    });
    expect(uppercase).toBe('uppercase');

    const capitalize = await page.locator('#capitalize').evaluate(el => {
      return window.getComputedStyle(el).textTransform;
    });
    expect(capitalize).toBe('capitalize');
  });

  test('VIS-013: Line height and letter spacing', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .spacing-box { width: 300px; border: 1px solid #ccc; margin: 10px 0; padding: 10px; }
            #line-height-1 { line-height: 1; }
            #line-height-1-5 { line-height: 1.5; }
            #line-height-2 { line-height: 2; }
            #letter-spacing-normal { letter-spacing: normal; }
            #letter-spacing-2px { letter-spacing: 2px; }
            #letter-spacing-5px { letter-spacing: 5px; }
            #word-spacing-normal { word-spacing: normal; }
            #word-spacing-10px { word-spacing: 10px; }
          </style>
        </head>
        <body>
          <div class="spacing-box" id="line-height-1">Line height 1.0<br>Second line</div>
          <div class="spacing-box" id="line-height-1-5">Line height 1.5<br>Second line</div>
          <div class="spacing-box" id="line-height-2">Line height 2.0<br>Second line</div>
          <p id="letter-spacing-normal">Normal letter spacing</p>
          <p id="letter-spacing-2px">Letter spacing 2px</p>
          <p id="letter-spacing-5px">Letter spacing 5px</p>
          <p id="word-spacing-normal">Normal word spacing here</p>
          <p id="word-spacing-10px">Word spacing 10px here</p>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify line-height
    const lineHeight1 = await page.locator('#line-height-1').evaluate(el => {
      return window.getComputedStyle(el).lineHeight;
    });
    expect(lineHeight1).toBeTruthy();

    const lineHeight2 = await page.locator('#line-height-2').evaluate(el => {
      const lineHeight = window.getComputedStyle(el).lineHeight;
      const fontSize = parseFloat(window.getComputedStyle(el).fontSize);
      return parseFloat(lineHeight) / fontSize;
    });
    expect(lineHeight2).toBeCloseTo(2, 0);

    // Verify letter-spacing
    const letterSpacing2px = await page.locator('#letter-spacing-2px').evaluate(el => {
      return window.getComputedStyle(el).letterSpacing;
    });
    expect(letterSpacing2px).toBe('2px');

    const letterSpacing5px = await page.locator('#letter-spacing-5px').evaluate(el => {
      return window.getComputedStyle(el).letterSpacing;
    });
    expect(letterSpacing5px).toBe('5px');

    // Verify word-spacing
    const wordSpacing10px = await page.locator('#word-spacing-10px').evaluate(el => {
      return window.getComputedStyle(el).wordSpacing;
    });
    expect(wordSpacing10px).toBe('10px');
  });

  test('VIS-014: Pseudo-elements (::before, ::after)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .pseudo-box {
              position: relative;
              padding: 20px;
              margin: 20px;
              background-color: #f0f0f0;
            }
            #before::before {
              content: "→ ";
              color: red;
              font-weight: bold;
            }
            #after::after {
              content: " ←";
              color: blue;
              font-weight: bold;
            }
            #both::before {
              content: "[ ";
              color: green;
            }
            #both::after {
              content: " ]";
              color: green;
            }
            #styled::before {
              content: "";
              position: absolute;
              left: 0;
              top: 0;
              width: 5px;
              height: 100%;
              background-color: #FF5722;
            }
          </style>
        </head>
        <body>
          <div class="pseudo-box" id="before">Before pseudo-element</div>
          <div class="pseudo-box" id="after">After pseudo-element</div>
          <div class="pseudo-box" id="both">Both pseudo-elements</div>
          <div class="pseudo-box" id="styled">Styled pseudo-element</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Verify pseudo-elements by checking computed content
    const beforeContent = await page.locator('#before').evaluate(el => {
      return window.getComputedStyle(el, '::before').content;
    });
    // Content should contain the right arrow character (U+2192)
    const beforeText = beforeContent.replace(/^"(.*)"$/, '$1');
    expect(beforeText).toContain('→');

    const afterContent = await page.locator('#after').evaluate(el => {
      return window.getComputedStyle(el, '::after').content;
    });
    // Content should contain the left arrow character (U+2190)
    const afterText = afterContent.replace(/^"(.*)"$/, '$1');
    expect(afterText).toContain('←');

    // Verify both pseudo-elements
    const bothBefore = await page.locator('#both').evaluate(el => {
      return window.getComputedStyle(el, '::before').content;
    });
    expect(bothBefore.replace(/^"(.*)"$/, '$1')).toContain('[');

    const bothAfter = await page.locator('#both').evaluate(el => {
      return window.getComputedStyle(el, '::after').content;
    });
    expect(bothAfter.replace(/^"(.*)"$/, '$1')).toContain(']');

    // Verify styled pseudo-element
    const styledBefore = await page.locator('#styled').evaluate(el => {
      const style = window.getComputedStyle(el, '::before');
      return {
        width: style.width,
        backgroundColor: style.backgroundColor
      };
    });
    expect(styledBefore.width).toBe('5px');
  });

  test('VIS-015: Pseudo-classes (:hover, :focus, :active)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .pseudo-button {
              padding: 10px 20px;
              margin: 10px;
              background-color: #4CAF50;
              color: white;
              border: none;
              cursor: pointer;
              transition: background-color 0.2s;
            }
            .pseudo-button:hover {
              background-color: #45a049;
            }
            .pseudo-button:active {
              background-color: #3d8b40;
            }
            .pseudo-input {
              padding: 10px;
              margin: 10px;
              border: 2px solid #ccc;
            }
            .pseudo-input:focus {
              border-color: #2196F3;
              outline: none;
            }
            .link:link { color: blue; }
            .link:visited { color: purple; }
            .link:hover { color: red; }
          </style>
        </head>
        <body>
          <button class="pseudo-button" id="button">Hover me</button>
          <input type="text" class="pseudo-input" id="input" placeholder="Focus me">
          <a href="#" class="link" id="link">Link with pseudo-classes</a>
        </body>
      </html>
    `;

    await page.goto(`data:text/html;charset=UTF-8,${encodeURIComponent(html)}`);

    // Get initial button color
    const initialButtonColor = await page.locator('#button').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(initialButtonColor).toContain('76, 175, 80'); // #4CAF50

    // Hover over button - verify hover state can be triggered
    await page.hover('#button');
    await page.waitForTimeout(300); // Wait for transition

    // Verify focus pseudo-class
    const initialInputBorder = await page.locator('#input').evaluate(el => {
      return window.getComputedStyle(el).borderColor;
    });

    await page.focus('#input');
    await page.waitForTimeout(100);

    const focusedInputBorder = await page.locator('#input').evaluate(el => {
      return window.getComputedStyle(el).borderColor;
    });
    expect(focusedInputBorder).toContain('33, 150, 243'); // #2196F3 when focused

    // Verify link pseudo-classes exist (color may vary by browser)
    const linkColor = await page.locator('#link').evaluate(el => {
      return window.getComputedStyle(el).color;
    });
    expect(linkColor).toBeTruthy();
  });

});
