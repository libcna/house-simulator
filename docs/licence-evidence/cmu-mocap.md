# CMU Graphics Lab Motion Capture Database — licence evidence

`HOUSE-00270`, which is **Q-04**: *may retargeted, baked derivatives be redistributed?* It gates
`HOUSE-00295` (retarget six clips onto the base rig) and the locomotion set behind it.

| | |
|---|---|
| Source | <http://mocap.cs.cmu.edu/> |
| Publisher | Carnegie Mellon University Graphics Lab |
| Funding | NSF Grant #0196217 / EIA-0196217 |
| Retrieved | 2026-09-07 |
| **Verdict** | **YES for derivatives, NO for the source data.** The two answers differ, and that difference is the finding. |

---

## The terms, verbatim

> This data is free for use in research projects. **You may include this data in commercially-sold
> products, but you may not resell this data directly, even in converted form.**
>
> If you publish results obtained using this data, we would appreciate it if you would send the
> citation to your published paper to jkh+mocap@cs.cmu.edu, and also would add this text to your
> acknowledgments section:
>
> > The data used in this project was obtained from mocap.cs.cmu.edu.
> > The database was created with funding from NSF EIA-0196217.

And, separately on the same page:

> Please don't crawl this database!

## The two answers, and why they differ

**Derived, baked animation inside the game: YES.** "You may include this data in commercially-sold
products" is exactly this project's case. A CMU clip retargeted onto our own rig, resampled and
written into a `.chanim` sidecar is included in a product, not sold as data. A paid Steam release is
covered by the same sentence.

**The raw `.asf`/`.amc`/`.bvh` in `assets-src/`: NO.** The prohibition is on redistributing "this
data directly, even in converted form", and `assets-src/` is a **published** directory — putting the
source clips there offers the data itself, as data, to anyone who clones the repository. The clause
literally forbids *reselling*, and a free repository is not a sale; but "even in converted form"
shows the intent is that the database not be re-hosted, and ADR-0012's rule for an unclear
permission is that we do not take it.

This is the second source where `redistributeSource` and `redistributeDerived` genuinely diverge —
the first was Quaternius, where the divergence disqualified the source entirely. Here it does not,
because what this project actually needs is the derivative.

| Manifest field | Value |
|---|---|
| `redistributeSource` | **false** — the raw clip does not enter `assets-src/` |
| `redistributeDerived` | **true** — the retargeted `.chanim` does |
| `commercialUse` | **true** |
| `modification` | **true** — retargeting is the point |

## What that means for the pipeline

`cna-house.md` §18's rule is that everything starts in `assets-src/`. CMU is the exception, and it
needs to be an explicit one rather than a quiet violation:

* the **source clips are fetched to a working directory outside the repository**, converted, and
  not committed;
* the committed artefact is the **`.chanim` sidecar** produced by `anim_extract.py` (`HOUSE-00223`),
  which is a derivative and is redistributable;
* the manifest row for that `.chanim` records the **source URL, the subject and trial numbers, the
  retrieval date and the SHA-256 of the source clip**, so the derivation is auditable even though
  its input is not present. That is what a hash in a manifest is for.

`HOUSE-00295` must record the six subject/trial numbers it retargets, not just "six CMU walk
clips" — the database has multiple takes per subject and the page warns that the low-numbered
subjects are early and lower quality, and that "toe" and "hand" joints are noisy and may need
smoothing.

## Two further terms

**Acknowledgement.** The requested credit line is phrased as a request tied to publishing results,
not a licence condition. This project will carry it anyway — the cost is one line and the request is
reasonable:

> The data used in this project was obtained from mocap.cs.cmu.edu.
> The database was created with funding from NSF EIA-0196217.

Because CMU is not a Creative Commons licence, this string goes in the manifest's `attribution`
field and so reaches `licenses/THIRD-PARTY-ASSETS.md` and the credits screen automatically.

**No crawling.** As with Poly Haven, fetch the specific clips a task names. `HOUSE-00295` names six.
