---
title: systemd targets
---

sessiond provides the following systemd targets:

Target                       | Started when
------                       | ------------
`graphical-lock.target`      | session is locked
`graphical-unlock.target`    | session is unlocked
`graphical-idle.target`      | session becomes idle
`graphical-unidle.target`    | session resumes activity
`user-sleep.target`          | system sleeps
`user-sleep-finished.target` | system resumes from sleeps
`user-shutdown.target`       | system shuts down
