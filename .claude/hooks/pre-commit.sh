#!/bin/bash
# Pre-commit hook for Ladybird development

echo "Running pre-commit checks..."

# Run clang-format
echo "Checking code formatting..."
git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|h)$' | xargs clang-format -i

# Run static analysis if available
if command -v clang-tidy &> /dev/null; then
    echo "Running clang-tidy..."
    # Add clang-tidy configuration here
fi

# Check for common security issues
echo "Running security checks..."
git diff --cached | grep -E "(TODO|FIXME|HACK|XXX).*security"

echo "Pre-commit checks complete!"
