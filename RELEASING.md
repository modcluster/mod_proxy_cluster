# Releasing new version

## Step-by-step

In order to release a new version, there are several steps that must happen:

 0. make sure all issues (in GitHub and in JIRA) targeted to the version are resolved
 1. create a pull request in which you change the [version string](https://github.com/modcluster/mod_proxy_cluster/blob/main/native/include/mod_proxy_cluster.h#L21) according to the format described in [another section](#version-naming-conventions)
 2. get review and merge the pull request
 3. release the version through GH release functionality with the tag corresponding to version from the pull request
 4. mark the corresponding version released in [JIRA](https://issues.redhat.com/projects/MODCLUSTER?release-page) as well


## Version naming conventions

Version string must have the following format:

* `major.minor.micro.Alpha<number>`
* `major.minor.micro.Beta<number>`
* `major.minor.micro.CR<number>`
* `major.minor.micro.Final`

where `<number>` is a mandatory numerical value starting from `1`. Any version marked `Final` must not have a number. If there
is any issue with such a version, just increase a micro version and go through the process again.
