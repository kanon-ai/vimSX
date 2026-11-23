"""Resolve data symbols against the actual link layout of each edition."""
import re

def data_base(root,mode):
    mapping=(root/'build'/(mode+'.map')).read_text()
    return int(re.search(r'^\s*([0-9A-F]+)\s+s__DATA\s',mapping,re.M)[1],16)
