# Page snapshot

```yaml
- generic [ref=e1]:
  - navigation [ref=e2]:
    - link "Go to Section 1" [active] [ref=e3] [cursor=pointer]:
      - /url: "#section1"
    - link "External Link" [ref=e4] [cursor=pointer]:
      - /url: http://example.com
    - link "Internal Link" [ref=e5] [cursor=pointer]:
      - /url: /internal-page
    - link "Email Link" [ref=e6] [cursor=pointer]:
      - /url: mailto:test@example.com
    - link "Phone Link" [ref=e7] [cursor=pointer]:
      - /url: tel:+1234567890
  - generic [ref=e8]: Section 1 Content
```