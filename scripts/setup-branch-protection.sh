#!/bin/bash

# Branch Protection Setup Script for CriptoGualet
# This script configures branch protection rules for the master branch
# Run this once after setting up the CI/CD pipeline

# Requirements:
# - GitHub CLI (gh) installed and authenticated
# - Repository owner or admin permissions

echo "Setting up branch protection for CriptoGualet..."
echo ""

# Check if gh is installed
if ! command -v gh &> /dev/null; then
    echo "ERROR: GitHub CLI (gh) is not installed."
    echo "Install from: https://cli.github.com/"
    exit 1
fi

# Check if authenticated
if ! gh auth status &> /dev/null; then
    echo "ERROR: Not authenticated with GitHub CLI."
    echo "Run: gh auth login"
    exit 1
fi

REPO="erickherrera/CriptoGualet"
BRANCH="master"

echo "Configuring protection for $REPO:$BRANCH"
echo ""

# Create branch protection rules
gh api repos/$REPO/branches/$BRANCH/protection \
  --method PUT \
  --input - <<EOF
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "Code Formatting",
      "Static Analysis", 
      "Build & Test (Windows) (Release)",
      "Build & Test (Windows) (Debug)"
    ]
  },
  "enforce_admins": false,
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": false,
    "required_approving_review_count": 1
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "required_conversation_resolution": false,
  "lock_branch": false,
  "allow_fork_syncing": true
}
EOF

if [ $? -eq 0 ]; then
    echo "✅ Branch protection configured successfully!"
    echo ""
    echo "Protection rules applied:"
    echo "  ✓ Require status checks to pass"
    echo "    - Code Formatting"
    echo "    - Static Analysis"
    echo "    - Build & Test (Release)"
    echo "    - Build & Test (Debug)"
    echo "  ✓ Require branches to be up to date"
    echo "  ✓ Require 1 approving review"
    echo "  ✓ Dismiss stale reviews on new commits"
    echo "  ✗ Force pushes not allowed"
    echo "  ✗ Branch deletion not allowed"
    echo ""
    echo "Next steps:"
    echo "  1. Push the CI workflow to master"
    echo "  2. Create a test PR to verify the checks work"
    echo "  3. Update CONTRIBUTING.md with CI requirements"
else
    echo "❌ Failed to configure branch protection"
    echo "Make sure you have admin access to the repository"
    exit 1
fi
