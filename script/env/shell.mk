mkdir  := mkdir -p
rm     := rm -f
rmdir  := rm -rf
echo   := echo
cp     := cp -rf
mv     := mv -f
cd     := cd
chmode := chmod

# Quote one argument for the POSIX shell.
shq = '$(subst ','"'"',$(1))'
