
The changes I have made are all relevant to the VI edit mode. The vi-edit
source seems mostly complete, so these changes are minimal. What is available
now is pretty useful. I will make further fixes as I notice the bugs (or
anyone else does).

A summary of changes follows:

1) two changes to turn on the vi-editing feature.
2) motion edit fix: back space and space bar now do something: have
the same behaviour as the 'h' and 'l' keys.
3) input edit fix: backspace key now works.

Known bugs (not fixed yet):

a) proviso on 1) and 2) above: the backspace key was hardwired to the
<delete> key. I have temporarily extended it to include the '^H' or
<backspace> key. I will generalize this to use whatever key is defined
as the erase character by stty.

b) the interrupt key does not clear the edit buffer. This means the next
edit command after an interrupt will be garbage.

c) Same sort of thing as b) occurs for the <EOF> key.

d) insertions near the beginning of an edit buffer seem to screw up the rest
of the buffer.

Known missing features:

a) search edit commands: [count]-, [count]+
b) text modification edit commands: P, p, *
c) r (replace) doesn't seem to work too well.
d) tilde does not do anything (should reverse upper to lower and vice versa)

e) ! in prompt doesn't get expanded to history number
