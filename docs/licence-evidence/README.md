# `docs/licence-evidence/`

Archived, verbatim excerpts from the authoritative pages a licence decision rests on.

`HOUSE-00261` and the Phase-4 verification tasks (`HOUSE-00262`…`HOUSE-00276`). ADR-0012's rule is
that a licence is recorded from **the authoritative source's own text**, never from an aggregator's
label and never from a search-engine snippet. This directory is where that text is kept, so a later
reader can see what we saw rather than re-visiting a page that may have changed.

One file per source, named for the source. Each carries:

* the exact URL and the retrieval date;
* the document's own version identifier, where it publishes one;
* the passages relied on, **quoted verbatim** and not paraphrased;
* what each passage was taken to mean for this project, kept clearly separate from the quote.

The licence *texts* themselves live in `licenses/<slug>/LICENCE.txt` — that is the legal instrument.
This directory holds the surrounding evidence: FAQs, terms pages, publisher statements. The two are
separate because they are different kinds of document and get cited differently.

**Negative findings are archived too.** A source rejected as unusable is worth more here than in
somebody's memory, and `cna-house.md` §19.2's table is a list of *candidates*, not of approvals.
