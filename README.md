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

Pull Requests
========
DO NOT directly push any code to this repository! If you do, you risk overriding someone else's work and getting yours overwritten.

Please fork this repository and have a version of your own.

Clone your forked repository, and commit and push to your forked repository in order to save progress.

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
