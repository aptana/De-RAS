# Contributing

De-RAS is private and in early design. This file sets the working rules for now.

## Workflow

1. Open an issue describing the change.
2. Create a branch from `main`: `feature/<short-name>` or `fix/<short-name>`.
3. Keep pull requests small and focused.
4. Add or update tests and docs with the code.
5. Open a pull request and fill in the template.

## Commit messages

Short imperative subject line, for example `Add normal depth boundary condition`.

## Engineering conventions

- SI units internally. Convert at the interface only.
- Every table, plot and map states its datum and CRS.
- Record the source and date for every input dataset in examples and tests.
- Label assumptions explicitly in code comments and docs.
- Hydraulic results should be checked against an analytical solution or a published benchmark where one exists.

## Clean-room rule

Implement from published theory, textbooks and public documentation. Do not copy code, assets or text from third-party software.
