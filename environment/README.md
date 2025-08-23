
# Constructing your Test/Development Environment

Your first challenge in GIOS is to construct a working environment for your
development. As such, this document will describe a number of techniques for
achieving this but **none** of this is required: we will evaluate your code in
an Ubuntu 20.04 environment but if you can develop your code in some other
fashion, feel free to do so.

Fall 2025 will be using an Ubuntu 20.04 environment for grading; we _recommend_
you create a compatible environment.  As part of the starter material that we
provide to you **some** of the binaries are pre-compiled for 64-bit AMD/Intel
compatible platforms using the Ubuntu 20.04 tools.
You don't need to use such a platform, but in that case
you will need to write the code that we've implemented in those pieces (you end
up writing it at some point in the projects where you need it).

Some students find this challenging; for courses where the system is just _a
tool_ it makes sense to deliver a working environment.  However, _this_ is a
systems course and as such your ability to install and configure systems is
considered to be a core competency - it's something you should already be
familiar with doing on other platforms.

Please keep in mind once you create your gatech github account and a project is released, it may take up to 24 hours to be added to the team.

## Contents

* [Introduction](#introduction)
* [What is in This Repository](#what-is-in-this-repository)
* [Easy Mode Install](#easy-mode-install)
* [Git](#git)
* [Submitting Code Outside the VM](#submitting-code-outside-the-vm)
* [Testing](#testing)
* [Verify Your Environmnet](#verify-your-environment)
* [Debugging](#debugging)

## Introduction

Note that this course does not provide you with a test framework for your code
development; we expect you to develop your _own_ tests; this work can (and
should) be done cooperatively.

Your code will be graded by an automated code evaluation framework.  That
framework runs in an **Ubuntu Linux 20.04 environment** and we will provide you
with some **binaries** that are compiled for Linux 20.04. We can't guarantee
these binaries will run on anything outside of the Ubuntu 20.04 environment,
which means 18.10, 19.04, 21.10, etc. will most likely **not work**.  It doesn't
work on Mac OS X platforms, or on Windows Subsystem for Linux (though WSL2
seems to work pretty well since it's just Ubuntu 20.04 inside a Hyper-V VM).

We do not run our grader in your environment: it will be running in a cloud
infrastructure environment where it will run on shared machines.  This leads to
potentially different behavior than you will see on your single-user desktop
system.  We use containers to isolate your work from everyone else, but all
those containers share resources.  We do not dictate that you do your
development in any particular way.  Find a way that makes you productive -
discuss it with your classmates and the instructional team.

**For those of you who don't have a preferred environment, please scroll down to
the ["Easy Mode Install"](#easy-mode-install) section below.**  This will
provide you with a configuration that is functional; it is _not_ the only viable
configuration.

You build your own tests and code in your own environment.  You may submit it to
the auto-grader a limited number of times.  We will only grade the **last**
submission prior to the deadline.

**Note**: Gradescope offers an "activate" button.  You may think this means we
will grade the version that you activated; it does not.  We've explained to
Gradescope _why_ this is the case so we will explain it to you: these projects
**have race conditions**.  If we let you activate a single solution, it is easy
for you to pick the one that worked best.  So, we use the **last submission**
and we reserve the right to re-run it.

Note:

1. We attempt to provide useful feedback, but there are far more ways to do
   things wrong, than to do it right (though there are multiple ways to do it
   right!)  If you don't understand the feedback, ask on Piazza (in a **public**
   thread).

2. We are allowed to return around 10KB of log output **for all tests
   combined**, so we must limit the log output **up to that limit** and then
   truncate the log beyond that.  If you see a failed test, but no feedback, fix
   one of the failing tests **with** feedback.  Once a test **passes** we won't
   return its feedback to you.  This permits us to send feedback on other
   failing tests.

3. The auto-grader isn't a test harness: it is more like delivering your
   software to your client, with each subsequent submission being your "patch"
   to the previous release.  Thus, you should build your own tests; we strongly
   encourage you to build scripts or other automated tests.  We encourage you to
   share your tests (so long as they do not give away the _solution_ to the
   project).

One of your first challenges in this class is to construct your development and
test environment. You have latitude on how you do this.  We note that for some
this can be a challenge, while others in the class will delight in their ability
to use an environment with which they are comfortable.  In the past we have
tried other approaches: thus far, this one has worked the best.

## What is in this Repository

This repository contains four files (in addition to this **README**) for your reference:

* **Vagrantfile** -- use this if you wish to provision with **Vagrant and VirtualBox**.
* **Dockerfile** -- use this if you wish to construct your own Docker image (you
  do not _have_ to build your own). Alternately, you may pull our publicly
  visible environment image on
  [docker.com](https://hub.docker.com/r/gtomscs6200/sum24-environment).
  See more on this in the next section.  Note that this relies upon **setup.sh**
  as well.
* **setup.sh** -- use this __with__ **Dockerfile** to construct your own docker image.
* **requirements.txt** -- this can be used with `pip` to ensure you have installed the required packages.

## Constructing Your Development Environment

There are many ways to construct your development environment.  Here are some
that students have found successful.

* If you are already running on a 64-bit AMD/Intel platform running Ubuntu
  20.04, then you can simply ensure the necessary packages are installed.
* If you are running on a different Linux version, Mac OS X or UNIX variant, you
  should be able to figure out how to install the necessary packages. Please
  understand, we'll answer questions if we can, but you are responsible for your
  development environment - we're not experts on every possible configuration
  (e.g., "Why doesn't this package work on my Raspberry Pi running Kali
  Linux?" - we're not going to know. Maybe someone else in the class does.)
* Beginning with Windows 10 (2004) or newer, Microsoft now supports a fully
  virtualized Ubuntu 20.04 environment. As far as we can tell, this is comparable
  to the Ubuntu 20.04 environment we are using (though we don't guarantee 100%
  compatibility). For versions prior to that there are known limitations that
  make it less useful.
* Apple's M1 & M2 based systems have known limitations that cause utilities like ptrace 
  and address sanitizer (recommended for projects) to be terminated abruptly. Students 
  are encouraged to use alternate platforms or utilities for projects.
* You may construct a virtual machine image. This could include:
  * Hyper-V on Windows.  VMWare and Virtual Box can be run with Hyper-V these
    days as well, though you have to enable VT-x extensions in Hyper-V (it's
    called [Nested
    Virtualization](https://redmondmag.com/articles/2020/02/24/nested-virtualization-windows-10-hyperv.aspx)
    and the link is to a decent article explaining how to set this up)
  * Virtual Box on Windows, Mac OS or Linux
  * KVM on Linux
  * ESXi. If you don't know what this is and don't recognize the term "bare
      metal hypervisor" then you probably don't want this particular option.
  * Xen. Again, this is a bare-metal hypervisor so if you don't know what that means, don't pursue this option.
  * VMWare Workstation. Note that this is a paid product. If you want to use
    Vagrant, you probably don't want to use VMWare.
  * Parallels.  Virtualization server for MacOS X. A [student discount](https://onthehub.com/parallels/) is available.
  * You may use Vagrant to construct your environment.  See [Easy Mode Install](#easy-mode-install) for details. Note: the Vagrantfile
    includes the ability to use the **hyperv** provider (on Windows 10/Server 2016+).  This is new, so please understand that this
    _may not work_.  We have not enabled the SMB sharing yet - if you get it to
    work, please let us know.  Keep in mind, Vagrant is not a hypervisor, it is
    a _provisioning_ mechanism.  It's similar to docker, but it provisions
    virtual machines rather than containers.
  * You may use a cloud environment:
    * Google will give you up to $300 in free time when you first sign up.
    * AWS can provide you with EC2 instances for a nominal fee (cheaper than buying a new computer).
      Georgia Tech has virtual machine resources that you can use, though nobody
      in class has reported doing this so far:
      [Instructional Computing at Georgia Tech](https://support.cc.gatech.edu/services/instructional-computing).
    * Microsoft Azure credits are available as part of the On the Hub software
      offerings from GT's CoC web store and it is certainly enough to run a
      virtual machine ($100 over the course of the year).
    * [Digital Ocean](https://www.digitalocean.com/products/droplets/) has "droplets" for $5 a month.
    * [Linode](https://www.linode.com/products/shared) specializes in Ubuntu VMs and has considerable flexibility for around $20 a month.
  * You may use the Ubuntu 20.04 environment on Windows 10 (2004 and more
      recent) as we mentioned above. Older versions have issues with address
      sanitizer.
  * You may use the Docker container (on [docker.com](https://hub.docker.com/r/gtomscs6200/fall25-environment) as gtomscs6200/fall25-environment). This public environment image closely matches our grading environment and may be configured to use git and deploy keys for easy access to your GitHub repo. See [source](https://docs.github.com/en/developers/overview/managing-deploy-keys).
    Or you may build your own Docker image using the Dockerfile included in this
    repository.  Note: the Dockerfile relies upon **setup.sh** to actually
    install the components.  This is similar to the script we use when setting up the
    graders. Some helpful references for Docker:
    [Get-started](https://docs.docker.com/get-started/) contains instructions on
    docker setup and [reference
    documentation](https://docs.docker.com/reference/) includes command-line and
    Dockerfile references.

  **This list is not exhaustive**. There are other, more exotic ways that you
  could build a working development environment. Feel free to experiment - and
  let us know how it goes so we can tell future students what works and what to
  avoid.

If you have an existing Ubuntu 20.04 base, you can install the requisite
software (using sudo apt-get install). You will also need add the following packages:

 ```bash
 # Add the course PPA
 curl -s --compressed "https://gt-cs6200.github.io/ppa/KEY.gpg" | sudo apt-key add -
 sudo curl -s --compressed -o /etc/apt/sources.list.d/gt-cs6200.list "https://gt-cs6200.github.io/ppa/gt-cs6200.list"

 # Update the repos
 sudo apt-get update

 # Install requirements
 sudo apt-get install -y build-essential git zip unzip software-properties-common python3-pip python3-dev gcc-multilib valgrind portmap rpcbind libcurl4-openssl-dev bzip2 libssl-dev llvm net-tools libtool pkg-config grpc-cs6200 protobuf-cs6200
 ```

Note: it is possible for you to have configured your own 20.04 environment so it
won't work properly, we've had that happen once (for project 4, which requires
inotify support be enabled). 

Finally, install the relevant python packages:

```shell
pip install --upgrade future cryptography pyopenssl ndg-httpsclient pyasn1
```

You may need to do this as root (or via elevation using sudo) depending upon
your environment. (the --upgrade on that line ensures you have a version of
cryptography that works with pyopenssl.  Without that, you will get warnings).
Note: it's not good practice to forcibly upgrade (for example, I'm using
Anaconda and I don't need to use sudo.)

But in the end, **the grading environment will be the docker container, based
upon Ubuntu 20.04 configured with the packages that we've listed**. We have found
that this environment definitely seems to stress student code in ways that cause
it to break. We suspect some of this is because it's running on a large MP
machine with plenty of resources and different behavior. Of course, the usual
practice is to blame the auto-grader for bugs in your code ("it works on my
machine"). One of the challenges of being a systems developer is learning how to
replicate the problems users of your code see. Try building some tests. Feel
free to share them with other students.

Bottom line: work in an environment that is comfortable for you. If you want to
build and test all of your code in a different environment more comfortable to
you, please feel free to do so. When you submit your code to the grader, it will
run it, test it, and provide you with feedback. You own your environment. We own
the test environment. We don't care that it works in your environment, we care
that it works in our environment.

Also, feel free to share a description of your environment on Piazza so other
people can learn from your own experiences.

## Easy Mode Install

_For some value of easy..._

The simplest way to construct your environment is to use Vagrant.  Just a few
steps and you should have a working virtual machine image with all the necessary
parts:

1. Assuming you don't have Vagrant installed already, download the latest
   [Vagrant binary](https://www.vagrantup.com/downloads.html) for your native
   platform. You may also need to download [Virtual
   Box](https://www.virtualbox.org/wiki/Downloads). For more details, please see
   the guides below (HT: Isabel Lupiani and Charles McGuinness from
   Computational Photography).
    * [Windows](https://docs.google.com/document/d/1FxuHsekpU5ng1ZxULZjpHg6u145fz8cie6kgH86y2uI/edit?usp=sharing)
    * [Mac OSX](https://docs.google.com/document/d/13IZnOzZtC5ZVmSy162fkpb44M3xsbhlIzjTAbfqDjjo/edit)
2. Copy the **Vagrantfile** from this repository to the top-level directory that you plan to use for the course.  For example:
    * CS6200/
      * Vagrantfile
      * pr1/
      * pr3/
      * pr4/

From any directory in which the `Vagrantfile` is present, you can start your
virtual machine with just the command `vagrant up` and connect to it with just
`vagrant ssh`.  Note that by default the vagrant machine will share the folder
containing the Vagrantfile at `/vagrant`. Therefore, your first command after
logging into the vagrant machine will almost always be `cd /vagrant`.  See the
[Vagrant documentation](https://www.vagrantup.com/docs/getting-started/) for
more details.

Most students have found this works well; however, sometimes something goes
wrong and the error isn't reported back in which case you end up with an
environment that doesn't quite work right. If that happens, you should destroy
the Vagrant image and recreate it - usually that does the trick.  Just remember,
anything _inside_ the virtual machine will be lost, so make sure you save
anything important first!

Vagrant is probably the least resource intensive environment - the image it
gives you with our default configuration file has no GUI (use ssh to access it -
easiest way to do this is vagrant ssh in the same directory as the Vagrantfile
after you start it with vagrant up).

You can share files between Vagrant (Virtual Box) and your host machine using
NFS, SMB or built-in guest sharing. Windows, MacOS, Linux and UNIX all have an
NFS client available.

You can install a windowing environment inside the guest machine. For example, I
have installed the X Windows pieces in the guest and an X server on the host (I
use MobaXterm on Windows). You can configure sshd to forward X as well. Then you
can open xterm windows, run CLion inside the VM while it displays on your host
monitor, etc.

Feel free to change the Vagrantfile to change the options if you'd like (e.g., if you don't want a gui window to show up.)
Login for the bento/ubuntu-20.04 environment is user vagrant password vagrant.

Use vagrant help to get a list of command.  Google is also your friend - this is
a commonly used deployment tool.

**Note:** One popular approach to this is to use VS Code along with its remote
development support.  With a modest amount of work it will edit the code within
the virtual machine and compile it for you.  Similarly, it can do this with
containers as well (so you edit the code outside the container but compile it
inside the container).

**Warning:** If you use Windows, there are some issues that arise when moving
files between Linux and Windows (or using shared storage).  One is that the line
endings of Windows include both a carriage return _and_ a newline character
while Linux uses only a newline for indicating the end of a line.  While many
Windows applications will handle either, most Linux applications only handle
newline characters.

## Testing your environment

Once you have your environment installed, you may be wondering how to verify you've done it right.  Of course, ultimately you'll want to use the actual project code, but we don't release that until the end of the first week.  So, we suggest you start off by downloading and compiling a test harness.  That might help you when it comes time to actually testing your own code.  Here is a suggested list:

* https://github.com/nemequ/munit
* https://github.com/clibs/cmocka
  Note: You may want to clone https://git.cryptomilk.org/projects/cmocka.git/ as the latest cmocka source has known issues.

**Note:** This step verifies your environment is setup correct for future class
project developments. This is optional and can be skipped if you use other
mechanisms for environment setup verification.

## Updates to this material

We may periodically update this as we find any issues.  You can monitor the
repository (track it) and you will be alerted to changes, or you can just
refresh it periodically with a `git pull`.

## Git

This and all assignments will be made available on the
[https://github.gatech.edu](https://github.gatech.edu) server.  To access the
code and import any updates, you will want to [install
git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git).  Git on
Windows is a little different, so you may want to consult this [guide for
Windows](https://docs.google.com/document/d/1_geDGrI0JlHHtnJY0P5pe6yU3E19NLEejdAOyDloqdQ/edit?usp=sharing)
(HT: Isabel Lupiani).

**Note: We are in the process of setting up the submission mechanism for Fall 2025.  We will advise you of the correct steps and update this document.**

Change to the directory you plan to use for the course.  Then clone this repository with the command

```console
$ git clone https://github.gatech.edu/gios-fall-24/pr1.git
...
```

(If this command fails with a complaint about "server certificate verification failed", then you might need to run

```console
$ GIT_SSL_NO_VERIFY=true git clone --recursive https://github.gatech.edu/gios-fall-24/pr1.git
...
$ cd pr1
$ git config http.sslVerify false
```

instead. Note we don't really recommend using git without SSL, but some people have had issues with misconfiguration in the past and this is a work around, albeit one that
pretends security is not an issue.)

**Note: the repositories won't be available until the project is released.  The first project is normally released shortly after the registration period ends.**

### Adding git origin/remote for committing your work

This was posted on the ML4T site Spring 2018 and it makes it much easier to manage your private repo and the public repo, in case changes are made to it.

1) Create a private repo named the same: gios-fall-24-pr1

2) Get the class repo

   ```console
   git clone --bare https://github.gatech.edu/gios-fall-24/pr1.git
   ```

3) mirror this to your private repo

   ```console
   cd pr1.git
   git push --mirror https://github.gatech.edu/<gtid>/gios-fall-24-pr1
   ```

4) You can now delete the directory pr1 if you wish

5) Now clone your private repo to your home dir

   ```console
   git clone https://github.gatech.edu/<gtid>/gios-fall-24-pr1.git
   ```

6) Next

   ```console
   cd gios-fall-24-pr1
   git remote add upstream https://github.gatech.edu/gios-fall-24/pr1.git
   #double check
   git remote -v
   ```

7) Now you can use it like this

   ```console
   git pull upstream main # the original repo
   git push origin main   # your repo
   ```

   Assuming main is the remote branch, please refer to [git pull documentation](https://git-scm.com/docs/git-pull) for a detailed description of the git pull command and its option. 
   If you do not specify the remote, it will default to the origin (your repo).

8) If you are scared of pushing to upstream you can disable pushing to upstream using

   ```console
   git remote set-url --push upstream PUSH_DISABLED
   ```

Source: [Duplicating a
Repository](https://help.github.com/enterprise/2.8/user/articles/duplicating-a-repository/)

## Submitting Code Outside the VM

We will use gradescope to submit your code. Detail instructions on submitting
code to gradescope will be included in the project ReadMe and/or in the piazza
project post.

## Testing

We do not provide you with a test harness.  We **strongly** recommend that you
build such an environment, as doing so will make your experience in the course
much more pleasant.
* You can construct your test environment using standard Ubuntu Linux tools, such as `netcat`.
* You can use unit test frameworks (there are several)
* You can code tests in other languages (e.g., Python) -- you may find it easier to implement the projects in your favorite language, which can form the basis of good tools.

Things to note about our grading platform:
* It is an evaluation interface, not a test harness.  To that end, we limit the number of times you can submit.
* All your submissions **must be your own work**.  We retrieve and save every submission and use them when reviewing your code provenance.  In other words,  don't use it to test someone else's code to see how it does as that is an honor code violation.
* Each automated test validates and verifies a specific control flow of the implemented system. Your submission must consistently pass the test for any credit for that functionality. Note that the automated tests will test some of these automatically, but graders may execute additional tests of these requirements.
* The projects have requirements that are not necessarily tested.  Thus, we will review your code to ensure you have met the project requirements.
* In general, we **do not** add points to your score, but we do reserve the
  right to deduct points for code that "passes the test" but does so in a way
  that does not meet project requirements.

## Verify Your Environment

Remember that comment about using your own (existing) 20.04 environment?  That
poor student has provided us with a suggestion for how you can test your
environment: make sure that inotify is working properly.

To do this, start with the
[inotify](https://www.man7.org/linux/man-pages/man7/inotify.7.html)
documentation.  Scroll to the bottom.  Look where it says "Program source".  Try
to convert that source into a file.  Try to compile that file.  See if the
resulting program works.  If you've never built a program on Linux before, find
a tutorial, like [How to Write and Run a C Program in
Linux](https://vitux.com/how-to-write-and-run-a-c-program-in-linux/#:~:text=%20How%20to%20Write%20and%20Run%20a%20C,see%20how%20the%20program%20is%20executed...%20More%20).

Use that source code from the inotify page.  Run the program.  See if it tells
you that inotify is working.  If it is, congratulations!  You're ready for the
projects.  If it runs but tells you that it can't work properly, you're going to
have an issue with Project 4.  Plenty of time to fix it because inotify is only
required for Project 4.

## Debugging

You will have bugs in your code.  You should use the tools available for debugging.  These include:
* gdb -- there's a bit of a learning curve with gdb, but knowing how to use a debugger is a useful skill.  Use it!  Note: there are other debuggers available as well (lldb, dbx, etc.) and you are welcome to use those as well.
* valgrind -- the code that we provide to you will generally be set up to use _AddressSanitizer_ which is a Google developed library for making sure your code is properly using memory (including stack space, dynamically allocated memory ("heap"), etc.)  Valgrind is another tool for doing a similar job.  **Valgrind and AddressSanitizer do not work together.**  To use valgrind you must use non-address sanitizer versions of your code.
* strace -- this will show you _what your code is doing_.
* hexdump -- you will be transferring binary files; showing them in a hex representation can be useful

Note: **This list is not exhaustive**

# Pull Requests

If you wish to contribute towards this document, please feel free to do so.

**We will accept pull requests (PRs) for this repository**  (note that we will _not_ accept them from the project repositories).

