[[_TOC_]]

# Introduction

First off, thank you for considering contributing to tr181-xpon and the [Ambiorix](https://gitlab.com/prpl-foundation/components/ambiorix) project. It's people like you that makes [Ambiorix](https://gitlab.com/prpl-foundation/components/ambiorix) such a great set of tools and libraries.

Following these guidelines helps to communicate that you respect the time of the developers managing and developing this open source project. In return, they should reciprocate that respect in addressing your issue, assessing changes, and helping you finalize your merge requests.

All members of our community are expected to follow our [Code of Conduct](https://gitlab.com/prpl-foundation/components/ambiorix/ambiorix/-/blob/main/doc/CODE_OF_CONDUCT.md). Please make sure you are welcoming and friendly in all of our spaces.

# Contribution Rules

- For new features/new code, make sure the code is covered by tests.
- Make sure that the gitlab ci/cd pipeline succeeds.
- Always start from a jira ticket. If no ticket exists for your new feature/bug fix, just create one [here](https://prplfoundationcloud.atlassian.net/secure/CreateIssue!default.jspa).
- Always document public API.
- Make sure new code is following the [coding guidelines](https://gitlab.com/prpl-foundation/components/ambiorix/ambiorix/-/blob/main/doc/CODING_GUIDELINES.md).

The creation of an issue is not needed when doing small and obvious fixes.
As a rule of thumb, changes are obvious fixes if they do not introduce any new functionality or creative thinking. As long as the change does not affect functionality, some likely examples include the following:

- Spelling / grammar fixes
- Typo correction, white space and formatting changes
- Comment clean up
- Extend/complete documentation

## Merge request

- One merge request per jira ticket. The commit in the MR should follow the following syntax: `Issue: PCF-XXX Commit title [suffix]`.
- Make sure your commit title is a good description of the changes done. Preferably add some small description to your commit as well.
- Make sure your commit message contains one of the following suffixes: `[fix], [change], [break], [remove], [sec], [depr], [new], [other]`.
    - [new] → To be used when a new feature is added.
    - [fix] → To be used for (bug)fixes.
    - [change] → To be used for changes to the behavior of your component. In other words: the same input gives another output.
    - [remove] → When the commit removes functionality.
    - [sec] → For security updates
    - [depr] → To indicate a feature will no longer be supported in the feature.
    - [break] → For breaking changes
    - [other] → For changes that have nothing to do with the code itself, like adding unit tests, documentation, gitlab ci, ...

# How to report a bug

Create a ticket in [jira](https://prplfoundationcloud.atlassian.net/secure/CreateIssue!default.jspa).

Make sure to answer these five questions:

1. What operating system and processor architecture are you using?
1. What did you do?
1. What did you expect to see?
1. What did you see instead?

# How to suggest a feature or enhancement

If you find yourself wishing for a feature that doesn't exist in Ambiorix (in general) or tr181-xpon (specific), you are probably not alone. Open an issue/ticket which describes the feature you would like to see, why you need it, and how it should work.
