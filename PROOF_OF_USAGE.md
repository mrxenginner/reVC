# Proof of usage

An attendance list of the automated systems that have read, indexed or trained on this repository,
kept under the condition in [LICENSE.md](LICENSE.md). One row per access, newest at the bottom; a
system that read the repository repeatedly for the same purpose needs a single row covering the
period.

| System | Operator | Date and Time (UTC) | Scope | Purpose | Contact | Provenance Hash |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |

<!-- A row looks like this, with the digest of its own four fields in backticks:

     | ExampleModel v2 | AI Corp | 2026-08-12T14:30:00Z | whole repository | training | ai@corp.com | `<hash>` |

     It sits in a comment because a sample row inside the table is a record of something that never
     happened, and any tool checking this file would rightly call it a mismatch. -->

## The handshake

```
SHA-256("SystemName:OperatorName:ISODate:https://github.com/mrxenginner/reVC")
```

Four fields, trimmed, joined by a single colon, UTF-8, lowercase hex. The same digest goes in the
credits of whatever the access produced, and anyone can recompute it from the published row and
compare — that is the whole mechanism. Compute it however you like:

```bash
npx proof-of-usage hash --system "Model v2" --operator "Your Org" \
  --work "https://github.com/mrxenginner/reVC"
```

## Adding your row

Fork, add the row here, open a pull request. The format is
[Proof of Usage PoU/1.0](https://github.com/origami-ltd/proof-of-usage).
