# ambientCG — licence evidence

`HOUSE-00263`. Candidate source for PBR materials (`cna-house.md` §19.2; the primary source for
`HOUSE-00296`'s 34-material base set).

| | |
|---|---|
| Source | <https://ambientcg.com/license> → redirects to <https://docs.ambientcg.com/license/> |
| Retrieved | 2026-09-07 |
| Publisher | ambientCG (Lennart Demes) |
| Licence | **CC0 1.0 Universal**, archived at `licenses/cc0-1.0/LICENCE.txt` |
| **Verdict** | **APPROVED** for use, redistribution and commercial release. No caveats. |

---

## What the page says, verbatim

> **License information**
>
> All ambientCG assets are provided under the Creative Commons CC0 1.0 Universal License.
> This applies to the downloadable asset files **and the material preview renders shown for each
> asset on the site**.

> **License Details**
>
> The Creative Commons CC0 license gives you near limitless freedom to use the materials, models and
> other assets:
>
> * You can copy, modify, distribute and perform the assets, even for commercial purposes, all
>   without asking permission.
> * **You can include the raw files in your project, for example a video game.**

> **Giving credit**
>
> You don't need to give credit but I would of course appreciate it, if you did it anyways.

*(Bold added on the two clauses this project relies on.)*

## Rights, as recorded in the manifest

| Question | Answer |
|---|---|
| Redistribute source | **yes** |
| Redistribute derived / compiled | **yes** |
| Commercial use | **yes** |
| Modification | **yes** |
| Attribution required | **no** — appreciated, not required |

## Two clauses that answer questions other sources leave open

**"You can include the raw files in your project, for example a video game."** This is the exact
question this project has to answer for every source, and ambientCG answers it in those words. It
matters more here than it would for a closed-source game, because `assets-src/` is published: this
repository distributes the *raw* asset files, not only a compiled game. A licence that permitted
shipping a build but not publishing the source file would be unusable for this project's structure
(see `quaternius-kenney-polypizza.md`, where exactly that happens).

**The preview renders are CC0 too**, stated explicitly. That is the opposite of Poly Haven, whose
ToS §4.1 reserves its example renders. So an ambientCG preview may be used in documentation or as a
placeholder; a Poly Haven one may not. Recorded because the two sources are otherwise interchangeable
and the difference is invisible from the licence label alone.

## Notes

* The suggested (optional) credit line, if ever used, is the publisher's own wording:
  `Created using <asset name> from ambientCG.com, licensed under the Creative Commons CC0 1.0
  Universal License.`
* Assets are also distributed over a documented API and a Nextcloud/WebDAV endpoint. No scraping
  prohibition equivalent to Poly Haven's ToS §3.2 was found on the licence or API documentation
  pages. `HOUSE-00296` should still fetch only the materials it names — that is this project's rule,
  not ambientCG's.
* CC0 is irrevocable, so a later policy change does not affect assets already obtained; the
  per-asset retrieval date in the manifest is what evidences which terms applied.
