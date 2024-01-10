# Contribution guidelines

Strawberry is an free and open-source project, it is possible and encouraged to participate in the development.
You can also participate by answering questions, reporting bugs or helping with documentation.


## Submitting a pull request

You should start by creating a fork of the Strawberry repository using the GitHub
fork button, after that you can clone the repository from your fork.
Replace "username" with your own.


### Clone the repository

    git clone git@github.com:username/strawberry.git
    cd strawberry


### Setup the remote

    git remote add upstream git@github.com:strawberrymusicplayer/strawberry.git


### Create a new branch

This creates a new branch from the master branch that you use for specific
changes.

    git checkout -b your-branch


### Stage changes

Once you've finished working on a specific change, stage the changes for
a specific commit.

Always keep your commits relevant to the pull request, and each commit as
small as possible.

    git add -p


### Commit changes

    git commit


### Commit messages

The first line should start with the name of the class that is changed
followed by a colon then a short explanation of the commit.
Don't use a trailing period after the first line.
If this change affects more than one class, omit the class name and write a
more general message.

You only need to include a main description (body) for larger changes
where the one line is not enough to describe everything.
The main description starts after two newlines, it is normal prose and
should use normal punctuation and capital letters where appropriate.
It should explain exactly what's changed, why it's changed,
and what bugs were fixed.

An example of the expected format for git commit messages is as follows:

```
StretchHeaderView: Set default section size

As of Qt 6.6.1, style changes are resetting the column sizes. To prevent this, we set a default section size.

Fixes #1328
```


### Push the changes to GitHub

Once you've finished working on the changes, push the branch
to the Git repository and open a new pull request.


    git push origin your-branch


### Update your fork's master branch

    git checkout master
    git pull --rebase origin master
    git fetch upstream
    git merge upstream/master
    git push origin master


### Update your branch

    git checkout your-branch
    git fetch upstream
    git rebase upstream/master
    git push origin your-branch --force-with-lease


### Rebase your branch

If you need fix any issues with your commits, you need to rebase your
branch to squash any commits, or to change the commit message.

    git checkout your-branch
    git log
    git rebase -i commit_sha~
    git push origin your-branch --force-with-lease


### Delete your fork

If you do not plan to work more on Strawberry, please delete your fork from GitHub
once the pull requests are merged.
