[← Music chapter](README.md)

# The school project and its reconstruction

Music generation was our team's final project at the MSU summer school
“Machine Learning, Neural Networks and Program Verification,” in Krasnovidovo,
24–31 August 2022. We worked on the neural-networks track and wanted to continue
music in MIDI form with a recurrent network.

The surviving repository was uploaded and edited in early 2023. Those dates
record the repository history, rather than moving the school project to 2023.
The notebook contains a manually expressed recurrent cell and a workflow using
pitch, step and duration. It is evidence of the team's project, not evidence that
one person independently authored every component.

The 2026 implementation starts afresh: a pinned score corpus, a tested event
representation, family-level holdouts, a PyTorch GRU, a Markov baseline, and
comparable audio examples. No historical musical code is reproduced here. Its
measurements describe the new implementation, developed with substantial AI
assistance. The earlier synthetic prototype remains in the private repository's
Git history; its numbers cannot be compared directly with this real-corpus study.

My original README's reminder survives in [the collection's marginalia](../../docs/marginalia.md).
