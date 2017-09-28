FFXI Server
========

This is a fork of darkstar project.

PLEASE READ THIS README BEFORE CONTRIBUTING TO THIS REPOSITORY.

Required additional files
========
You need to add sendgrid.conf file in conf folder in order to be able to create game accounts. It can be found in google drive, "FFXI Project Coding."

Also you need to use xiloader.exe from the client side to be able to sign up. It can be found in google drive, "FFXI Project Coding."

Managing Repository
========
Refrain from editing the repository on our B1 laptop directly. 

Ideally we would use other test servers (or your own), push your changes to your forked repo, and then create pull request online.

On B1 laptop, we would want to only git pull changes we made on the repository.

Collaboration on Github - Best Practices
========
Please refer to this guidelines: 

https://github.com/ideaconsult/etc/wiki/GitHub-Collaboration-Best-Practices

https://github.com/agis/git-style-guide

I tried to explain this in plain English below. Please read carefully!

Pull Requests
========
DO NOT directly push any code to this repository! If you do, you risk overriding someone else's work and getting yours overwritten.

Please fork this repository and have a version of your own.

If there is something you need to work on the server (adding file, fixing code, whatever), there should be a new branch for that task. You may ask who is in charge of bmscoordinators/FFXI-Server to create one for you, or you can create your own on your forked repository.  (See how to do this here: https://stackoverflow.com/questions/19944510/create-a-remote-branch-on-github) If you don't know what branches are and how they work, please refer to this: https://www.atlassian.com/git/tutorials/using-branches

Clone your forked repository, checkout your branch, commit and push your code to your branch.

When you want to bring the changes to the main repository, create pull request following this guideline: https://help.github.com/articles/creating-a-pull-request-from-a-fork/

Commits should contain a descriptive name for what you are modifying

Please test your code before committing changes/submitting a pull request.

Maintain your forked repo up-to-date
========
Use git pull to download changes on the bmscoordinators/FFXI-Server to your forked version, and do so whenever we accept git pull request! Use git stash or git rebase to preserve your work after downloading changes. Refer to this guideline: https://stackoverflow.com/questions/3903817/pull-new-updates-from-original-github-repository-into-forked-github-repository

Adding large files
========
Use git lfs for adding files larger than 100MB. https://git-lfs.github.com/

git lfs track "large_filename"

git add "large_filename"

git commit -m "Add large_filename"

git push

Wiki
========
Darkstar wiki can be found here:
https://wiki.dspt.info/
