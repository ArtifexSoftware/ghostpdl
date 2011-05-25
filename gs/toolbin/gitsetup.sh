#!/bin/sh

git config core.whitespace "space-before-tab,tab-in-indent,trailing-space"
git config branch.autosetuprebase always
git config branch.master.rebase true
git config push.default tracking
git config alias.update 'pull --rebase'
git config alias.up 'pull --rebase'
git config alias.logg "log --graph --oneline --decorate --all"
git config ui.color true

echo "Git configuration (local):"
git config -l

echo
echo "Git configuraton (global):"
git config --global -l

echo
echo You may want to consider the following if they are not correct above:
echo   git config --global user.name "Your Name"
echo   git config --global user.email "your.name@artifex.com"
