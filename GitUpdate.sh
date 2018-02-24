#!/bin/bash
#Add changed files to Robopot Github Repository
git add .
git status
git commit
git push
echo "Github master has been updated"
