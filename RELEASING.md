# Releasing new version

## Step-by-step

In order to release a new version, there are several steps that must happen:

 0. make sure all issues (in GitHub and in JIRA) targeted to the version are resolved
 1. create a pull request in which you change the [version string](https://github.com/modcluster/mod_proxy_cluster/blob/main/native/include/mod_proxy_cluster.h#L21) according to the format described in [another section](#version-naming-conventions)
 2. get review and merge the pull request
 3. release the version through GitHub release functionality with a new tag corresponding to the version string from
 the pull request (create the tag during the release through the release dialog)
 4. mark the corresponding version[^1] released in [JIRA](https://issues.redhat.com/projects/MODCLUSTER?release-page) as well


## Version naming conventions

Version string must have the following format:

* `major.minor.micro.Dev`
* `major.minor.micro.Alpha<number>`
* `major.minor.micro.Beta<number>`
* `major.minor.micro.CR<number>`
* `major.minor.micro.Final`

where `<number>` is a mandatory numerical value starting from `1`. Any version marked `Final` must
not have a number. If there is any issue with such a version, just increase a micro version and go
through the process again.

Versions in between of `x.y.z.Final` and next `a.b.c.Alpha1` will be called `a.b.c.Dev`.

[^1]: The corresponding version within the JIRA project has `native-` prefix. For example version `2.0.0.Final` in this repository will be `native-2.0.0.Final` in the JIRA project.
