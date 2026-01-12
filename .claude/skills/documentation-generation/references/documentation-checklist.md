# Documentation Review Checklist

Use this checklist when reviewing documentation changes or creating new documentation.

## General Quality

### Content

- [ ] **Purpose is clear**: Reader knows what this document is about in first paragraph
- [ ] **Audience is appropriate**: Written for the intended reader (user vs developer)
- [ ] **Scope is defined**: Document states what it covers and what it doesn't
- [ ] **Examples are relevant**: Code examples match real-world use cases
- [ ] **Information is current**: Content reflects latest code/features
- [ ] **No dead ends**: All referenced features/APIs are documented

### Structure

- [ ] **Logical organization**: Content flows naturally from simple to complex
- [ ] **Proper heading hierarchy**: One H1, proper H2/H3/H4 nesting
- [ ] **Table of contents**: Included for documents >3 sections
- [ ] **Sections are balanced**: No extremely long or extremely short sections
- [ ] **Related docs linked**: Cross-references to complementary documentation

### Writing Quality

- [ ] **Grammar and spelling**: No errors (use spell-checker)
- [ ] **Consistent terminology**: Same concepts use same terms throughout
- [ ] **Active voice**: "Use TRY() to propagate errors" not "Errors are propagated using TRY()"
- [ ] **Concise**: No unnecessary words ("in order to" → "to")
- [ ] **Technical accuracy**: All technical details are correct

## Markdown Formatting

### Structure

- [ ] **One H1 heading**: Document title only
- [ ] **Proper hierarchy**: No skipped levels (H2 → H4)
- [ ] **Blank lines**: After headings, around code blocks, between sections
- [ ] **Line length**: ~120 characters maximum (except URLs)
- [ ] **Consistent markers**: Same list marker style throughout (-)

### Code

- [ ] **Language specified**: All code blocks have language tags
- [ ] **Syntax valid**: Code blocks use correct syntax
- [ ] **Examples work**: Code examples compile/run successfully
- [ ] **Inline code**: Technical terms use backticks (`ErrorOr`)
- [ ] **Output separated**: Command output distinguished from commands

### Links

- [ ] **Relative paths**: Internal links use relative paths
- [ ] **No broken links**: All links point to existing files/sections
- [ ] **Anchor links work**: Heading anchors are correct
- [ ] **External links valid**: HTTPS links are accessible
- [ ] **Descriptive text**: Link text describes destination

### Images

- [ ] **Alt text present**: All images have descriptive alt text
- [ ] **Paths correct**: Images load correctly
- [ ] **Appropriate format**: SVG for diagrams, PNG/JPG for screenshots
- [ ] **Reasonable size**: Images are <1MB (optimize if larger)
- [ ] **Readable**: Text in images is legible at normal zoom

## Code Documentation

### Comments

- [ ] **Explain why**: Comments explain reasoning, not what (code shows what)
- [ ] **Proper sentences**: Start with capital, end with period
- [ ] **No redundancy**: Don't comment obvious code
- [ ] **Spec links**: Web standards include spec URLs
- [ ] **TODOs marked**: Use `FIXME:` or `TODO:` format

### Doxygen (if used)

- [ ] **Brief descriptions**: One-line `@brief` for public APIs
- [ ] **Parameter docs**: All parameters documented with `@param`
- [ ] **Return values**: `@return` describes both success and error cases
- [ ] **Error conditions**: `@throws` lists all possible errors
- [ ] **Examples included**: Complex APIs have `@code` examples
- [ ] **Cross-references**: Related functions linked with `@see`

### Function Signatures

- [ ] **Descriptive names**: Function names clearly indicate purpose
- [ ] **Parameter names**: Meaningful names (not `x`, `y`, `z`)
- [ ] **Return types**: ErrorOr for fallible operations
- [ ] **Const correctness**: Const where appropriate

## API Documentation

### Coverage

- [ ] **All public APIs**: Every public class/function documented
- [ ] **Parameters described**: All parameters have descriptions
- [ ] **Return values**: Return type and meaning documented
- [ ] **Error codes**: All possible errors listed and explained
- [ ] **Thread safety**: Concurrency constraints documented
- [ ] **Ownership**: Memory ownership semantics clear

### Examples

- [ ] **Basic usage**: Simple example showing common case
- [ ] **Advanced usage**: Example for complex scenarios
- [ ] **Error handling**: Shows how to handle errors
- [ ] **Working code**: Examples compile and run
- [ ] **Realistic**: Examples match actual use cases

### Organization

- [ ] **Grouped logically**: Related functions grouped together
- [ ] **Progressive disclosure**: Simple concepts before complex
- [ ] **Quick reference**: Summary table or synopsis
- [ ] **Migration guides**: For breaking changes

## Diagrams and Visuals

### Mermaid Diagrams

- [ ] **Appropriate type**: Right diagram type for concept
- [ ] **Readable**: Text is legible, not cluttered
- [ ] **Labeled clearly**: All nodes/edges have descriptive labels
- [ ] **Consistent style**: Similar diagrams use similar styling
- [ ] **Source included**: .mmd source file in repository
- [ ] **Generated images**: PNG/SVG generated and committed

### Architecture Diagrams

- [ ] **Shows structure**: Components and their relationships clear
- [ ] **Appropriate detail**: Not too abstract, not too specific
- [ ] **Legend included**: If using colors/symbols
- [ ] **Up-to-date**: Reflects current architecture

### Sequence Diagrams

- [ ] **Clear flow**: Message flow is easy to follow
- [ ] **Complete**: Shows entire interaction, not partial
- [ ] **Annotated**: Notes explain non-obvious steps

## Feature Documentation

### User Guides

- [ ] **Quick start**: Gets user running in <5 minutes
- [ ] **Prerequisites**: Required dependencies listed
- [ ] **Installation**: Step-by-step build/install instructions
- [ ] **Configuration**: All options documented
- [ ] **Examples**: Real-world usage examples
- [ ] **Troubleshooting**: Common issues and solutions

### Technical Specs

- [ ] **Requirements**: Functional and non-functional requirements
- [ ] **Design rationale**: Why this approach was chosen
- [ ] **Alternatives considered**: Why rejected
- [ ] **Trade-offs**: Benefits and costs documented
- [ ] **Implementation notes**: Important details for developers

## Changelog

### Entries

- [ ] **Categorized**: Added/Changed/Deprecated/Removed/Fixed/Security
- [ ] **Specific**: "Fix memory leak in X" not "Fix bugs"
- [ ] **User-focused**: Written for users, not developers
- [ ] **Issue links**: Link to issues/PRs where appropriate
- [ ] **Breaking changes**: Clearly marked and explained

### Format

- [ ] **Keep a Changelog**: Follows keepachangelog.com format
- [ ] **Semantic versioning**: Version numbers follow semver
- [ ] **Dates included**: Release dates in YYYY-MM-DD format
- [ ] **Unreleased section**: Exists for upcoming changes
- [ ] **Comparison links**: Links to diff between versions

## Testing Documentation

### Test Documentation

- [ ] **Purpose clear**: What the test validates
- [ ] **Setup described**: How to prepare test environment
- [ ] **Execution steps**: How to run tests
- [ ] **Expected results**: What success looks like
- [ ] **Failure debugging**: How to diagnose failures

## Platform-Specific

### Multi-Platform Docs

- [ ] **Platform coverage**: All supported platforms documented
- [ ] **Differences noted**: Platform-specific behavior explained
- [ ] **Separate sections**: Clear sections for Linux/macOS/Windows
- [ ] **Default clear**: Which platform is the default/primary

## Accessibility

### Inclusive Language

- [ ] **Gender-neutral**: Use "they" not "he" or "she"
- [ ] **Non-ableist**: Avoid "sanity check", "blind", "cripple"
- [ ] **Cultural sensitivity**: Avoid culturally-specific idioms
- [ ] **Plain language**: Avoid jargon where possible

### Structure

- [ ] **Semantic markup**: Proper heading levels (for screen readers)
- [ ] **Descriptive links**: "See the build guide" not "click here"
- [ ] **Image alt text**: Meaningful alt text for images
- [ ] **Code descriptions**: Code explained in text too

## Maintenance

### Sustainability

- [ ] **Maintainer noted**: Contact/team responsible for doc
- [ ] **Last updated**: Date of last significant update
- [ ] **Version info**: Which code version this applies to
- [ ] **Review schedule**: When this should be reviewed next

### Automation

- [ ] **CI checks**: Broken links checked in CI
- [ ] **Auto-generated**: Generated docs (like API) in CI
- [ ] **Pre-commit hooks**: Linting/formatting automated
- [ ] **Diagram generation**: Mermaid → PNG/SVG automated

## Publishing

### Pre-Publish

- [ ] **Spell-checked**: Run spell-checker
- [ ] **Lint-checked**: Run markdownlint or similar
- [ ] **Links verified**: All links tested
- [ ] **Rendered correctly**: Preview in markdown viewer
- [ ] **Mobile-friendly**: Readable on mobile devices

### Post-Publish

- [ ] **Announced**: Significant docs announced to team
- [ ] **Discoverable**: Linked from appropriate places
- [ ] **Search indexed**: Searchable if using doc search
- [ ] **Feedback enabled**: Way for readers to report issues

## Final Checks

### Before Committing

- [ ] **Reviewed own changes**: Read through all changes
- [ ] **Examples tested**: Ran all code examples
- [ ] **Links clicked**: Tested all links work
- [ ] **Spell-checked**: No typos or grammar errors
- [ ] **Formatted**: Consistent formatting throughout

### Pull Request

- [ ] **Description clear**: PR explains what docs changed and why
- [ ] **Screenshots**: For visual changes, include before/after
- [ ] **Reviewer assigned**: Someone familiar with topic reviews
- [ ] **CI passes**: All automated checks pass
- [ ] **Feedback addressed**: Reviewer comments resolved

## Review Guidelines

### For Reviewers

When reviewing documentation PRs:

1. **Check accuracy**: Technical details are correct
2. **Check completeness**: All necessary information present
3. **Check clarity**: Understandable to target audience
4. **Check consistency**: Matches existing doc style
5. **Check formatting**: Markdown is well-formatted
6. **Check links**: All links work
7. **Check examples**: Code examples are correct
8. **Suggest improvements**: Offer constructive feedback

### For Authors

When addressing review feedback:

1. **Accept gracefully**: Reviewers are trying to help
2. **Ask questions**: If feedback unclear, ask for clarification
3. **Explain choices**: If disagreeing, explain your reasoning
4. **Iterate**: Multiple review rounds are normal
5. **Thank reviewers**: Acknowledge their time and effort

## Documentation Types Checklist

### README.md

- [ ] Project name and description
- [ ] Quick start section
- [ ] Installation instructions
- [ ] Basic usage examples
- [ ] Link to detailed documentation
- [ ] Contributing guidelines (or link)
- [ ] License information

### Architecture Documentation

- [ ] System overview
- [ ] Component diagram
- [ ] Data flow description
- [ ] Key design decisions
- [ ] Trade-offs explained
- [ ] Future improvements noted

### API Reference

- [ ] All public classes documented
- [ ] All public methods documented
- [ ] Parameters described
- [ ] Return values explained
- [ ] Error conditions listed
- [ ] Usage examples included
- [ ] Thread safety noted

### User Guide

- [ ] Prerequisites listed
- [ ] Installation steps
- [ ] Configuration options
- [ ] Usage examples
- [ ] Troubleshooting section
- [ ] FAQ section

### Tutorial

- [ ] Learning objectives stated
- [ ] Prerequisites listed
- [ ] Step-by-step instructions
- [ ] Expected output shown
- [ ] Exercises for practice
- [ ] Next steps suggested

## Common Issues

Watch out for these common documentation problems:

- [ ] **Stale examples**: Code examples don't work with current version
- [ ] **Missing context**: Assumes knowledge reader may not have
- [ ] **Jargon overload**: Too many technical terms without explanation
- [ ] **Poor organization**: Information hard to find
- [ ] **Incomplete error handling**: Doesn't show how to handle errors
- [ ] **Platform assumptions**: Assumes Linux when also runs on macOS/Windows
- [ ] **Broken links**: Links to moved/deleted files
- [ ] **Outdated screenshots**: Images don't match current UI
- [ ] **Missing prerequisites**: Doesn't list required dependencies
- [ ] **Unclear scope**: Doesn't state what's covered and what's not

---

## Using This Checklist

**For new documentation**:
1. Use relevant sections as you write
2. Review entire document against checklist before submitting
3. Have someone else review using checklist

**For documentation reviews**:
1. Go through appropriate sections for doc type
2. Mark items that need attention
3. Provide specific feedback on marked items

**For documentation maintenance**:
1. Review periodically (quarterly/annually)
2. Update stale information
3. Add missing sections from checklist

Not all items apply to all documents. Use judgment to determine which checks are relevant.
