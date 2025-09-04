#!/bin/bash

echo "🔒 Testing Security Scan Configuration"
echo "======================================"

# Check if suppression.xml exists and is valid
echo "1. Checking suppression.xml..."
if [ -f "suppression.xml" ]; then
    echo "✅ suppression.xml exists"
    if xmllint --noout suppression.xml 2>/dev/null; then
        echo "✅ suppression.xml is valid XML"
    else
        echo "❌ suppression.xml is not valid XML"
    fi
else
    echo "❌ suppression.xml not found"
fi

# Check if dependency-check-config.xml exists and is valid
echo ""
echo "2. Checking dependency-check-config.xml..."
if [ -f "dependency-check-config.xml" ]; then
    echo "✅ dependency-check-config.xml exists"
    if xmllint --noout dependency-check-config.xml 2>/dev/null; then
        echo "✅ dependency-check-config.xml is valid XML"
    else
        echo "❌ dependency-check-config.xml is not valid XML"
    fi
else
    echo "❌ dependency-check-config.xml not found"
fi

# Check GitHub Actions workflows
echo ""
echo "3. Checking GitHub Actions workflows..."
if [ -d ".github/workflows" ]; then
    echo "✅ .github/workflows directory exists"
    workflow_count=$(find .github/workflows -name "*.yml" -o -name "*.yaml" | wc -l)
    echo "✅ Found $workflow_count workflow files"
    
    # Check for security-related workflows
    if [ -f ".github/workflows/dependency-scan.yml" ]; then
        echo "✅ dependency-scan.yml exists"
    fi
    if [ -f ".github/workflows/security-scan-simple.yml" ]; then
        echo "✅ security-scan-simple.yml exists"
    fi
else
    echo "❌ .github/workflows directory not found"
fi

# Check for basic security issues in source code
echo ""
echo "4. Running basic security checks..."
dangerous_funcs=$(find . -name "*.cpp" -o -name "*.c" -o -name "*.h" | xargs grep -l "strcpy\|sprintf\|gets\|scanf" 2>/dev/null || true)
if [ -n "$dangerous_funcs" ]; then
    echo "⚠️  Found potentially dangerous C functions:"
    echo "$dangerous_funcs"
else
    echo "✅ No dangerous C functions found"
fi

echo ""
echo "🔒 Security scan configuration test completed!"
echo ""
echo "Next steps:"
echo "1. Commit these files to your repository"
echo "2. The GitHub Actions should now work properly"
echo "3. Check the Actions tab for security scan results"
