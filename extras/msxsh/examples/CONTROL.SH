# MSXSH 0.2: run from DOS with MSXSH CONTROL.SH
# Requires GREP.COM. No disk files are written.
NAME=MSX
if [ -n "$NAME" ]; then
    echo "Hello $NAME!"
else
    echo "No name"
fi

for fruit in apple banana cherry; do
    if [ "$fruit" = banana ]; then
        echo "Special: $fruit"
    else
        echo "Fruit: $fruit"
    fi
done

for pattern in apple orange; do
    if echo "apple banana" | grep "$pattern"; then
        echo "Found: $pattern"
    else
        echo "Not found: $pattern"
    fi
done

# Exported values are also visible to external COM programs.
export DEMO_MODE=sample
echo "Mode: $DEMO_MODE"
unset NAME fruit pattern DEMO_MODE
exit 0
