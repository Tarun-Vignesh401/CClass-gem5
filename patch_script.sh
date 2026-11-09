
git -C gem5 checkout c8222cc67a399bfc01e8658dd14b30d5bfd634f9
git -C gem5 status
git -C gem5 apply --check ../cclass.patch
git -C gem5 apply ../cclass.patch
