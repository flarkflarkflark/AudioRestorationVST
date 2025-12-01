#!/bin/bash
# Setup script to push Audio Restoration Suite to GitHub

echo "=== Audio Restoration Suite - GitHub Setup ==="
echo ""

# Step 1: Configure git identity
echo "Step 1: Configure Git Identity"
read -p "Enter your name (for git commits): " GIT_NAME
read -p "Enter your email: " GIT_EMAIL

git config user.name "$GIT_NAME"
git config user.email "$GIT_EMAIL"

echo "✓ Git identity configured"
echo ""

# Step 2: Create initial commit
echo "Step 2: Creating initial commit..."
git commit -m "Initial commit: Audio Restoration Suite v1.0

Professional audio restoration VST3 and Standalone application

Features:
- 10-band graphic EQ (31Hz - 16kHz)
- Rumble filter (5-150Hz high-pass)
- Hum filter (40-80Hz notch)
- Difference mode (hear what's being removed)
- Click removal framework (crossfade technique ready)
- Noise reduction framework (FFT structure in place)
- Fully resizable GUI (640x480 to 2560x1440)

Built with JUCE framework
Company: Flark Audio"

echo "✓ Initial commit created"
echo ""

# Step 3: Instructions for GitHub
echo "Step 3: Create GitHub Repository"
echo ""
echo "Please do the following:"
echo "1. Go to: https://github.com/flarkflarkflark"
echo "2. Click 'New' to create a new repository"
echo "3. Name: AudioRestorationVST"
echo "4. Description: Professional audio restoration VST3/Standalone - Click removal, noise reduction, filters & EQ"
echo "5. Choose Public or Private"
echo "6. DO NOT initialize with README (we already have one)"
echo "7. Click 'Create repository'"
echo ""
read -p "Press Enter after you've created the repository on GitHub..."

# Step 4: Add remote and push
echo ""
echo "Step 4: Pushing to GitHub..."
git remote add origin https://github.com/flarkflarkflark/AudioRestorationVST.git
git branch -M main
git push -u origin main

echo ""
echo "✓ Done! Your repository is now on GitHub!"
echo "Visit: https://github.com/flarkflarkflark/AudioRestorationVST"
