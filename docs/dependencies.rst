Dependencies
============

As Mesa is used in most distributions by default choosing and adding
dependencies demands for special consideration before accepting patches adding
new ones.

Sometimes we also have in tree experiments where distributions and users are
encouraged to not use them by default, either by being hidden behind
environmental variables or some other way which doesn't remove the default
at compile time. As long as there is always a **default** available to users
which is also the generally preferred option, some of the following rules can
be ignored on a case by case basis.

Open Development Model
----------------------

All externel dependencies except OS provided core libraries must follow an Open
Development Model where new patches are reviewed and discussed in public.
External contributions need to be able to engage with the project in public and
there has to be a streamlined path for getting patches accepted.

This is something which is required by some distribution miantainers in order
to make their life easier and be able to handle bugs inside mesa and the
external dependency.

Distribution acceptance
-----------------------

All dependencies relevant for Linux builds need to have some sort of acceptence
in the biggest distributions either by being shipped already or maintainer
having stated in public that they are willing to accept new packaging of the
dependencies in question.

Dependencies distribution maintainer are not willing to add to their Live
Images are also out of question.

LLVM
----

Dependencies being an LLVM fork or can't keep up with LLVM releases are not
acceptable. We can't require from distribution to package multiple LLVM forks
nor do we want to get held back by dependencies from using the latest LLVM
releases.

Specifically for backend compilers there is a strong preference of writing an
in tree NIR based compiler to keep compilation time minimal and being able to
fix bugs in mesa without having to rely on a new LLVM release.
