# MSXSH demo: run from the MSX-DOS prompt with MSXSH DEMO.SH
# Requires GREP.COM in the current directory or PATH.
echo === MSXSH demo ===
echo Current directory:
pwd

# Environment variable and quotes
export MSXSH_DEMO=Hello
echo "$MSXSH_DEMO MSX!"
unset MSXSH_DEMO

# Conditional execution using && and ||
true && echo Success branch
false || echo Failure branch

# Small mapper RAM pipeline
echo "apple banana" | grep banana | grep -n apple
echo Pipeline status: $?

# No match returns status 1
echo apple | grep orange
echo No-match status: $?
echo === Done ===
exit 0
