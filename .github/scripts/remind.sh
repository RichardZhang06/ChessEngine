#!/usr/bin/env bash
# Remind $ASSIGNEE about their open tickets by @-mentioning them in one tracking issue (one notification a day).
# Does nothing when they have no open tickets. Set DRY_RUN=1 to print what would be posted instead of posting.
set -euo pipefail

: "${ASSIGNEE:?set ASSIGNEE to a GitHub username}"
: "${GH_REPO:?set GH_REPO to owner/name}"
DRY_RUN="${DRY_RUN:-0}"
LABEL="reminder"
TITLE="Reminder: open tickets for @${ASSIGNEE}"

# Open tickets assigned to them, leaving out the tracking issue itself
tickets=$(gh issue list --repo "$GH_REPO" --assignee "$ASSIGNEE" --state open --limit 100 \
  --json number,title --jq "[.[] | select(.title != \"$TITLE\")]")
count=$(jq length <<<"$tickets")

if [ "$count" -eq 0 ]; then
  echo "@$ASSIGNEE has no open tickets, nothing to remind."
  exit 0
fi

list=$(jq -r '.[] | "- [ ] #\(.number) \(.title)"' <<<"$tickets")
body="Hi @${ASSIGNEE}, you still have ${count} open ticket(s) assigned to you:

${list}

Close a ticket (or ask to have it reassigned) once it's done and it drops off this list. This reminder is posted daily until none are left."

if [ "$DRY_RUN" = "1" ]; then
  echo "--- DRY RUN: would post to the tracking issue \"$TITLE\" ---"
  echo "$body"
  exit 0
fi

existing=$(gh issue list --repo "$GH_REPO" --label "$LABEL" --state open --search "\"$TITLE\" in:title" \
  --json number --jq '.[0].number // empty')

if [ -n "$existing" ]; then
  gh issue comment "$existing" --repo "$GH_REPO" --body "$body"
  echo "Commented on #$existing"
else
  gh label create "$LABEL" --repo "$GH_REPO" --color fbca04 --description "Automated daily reminder" 2>/dev/null || true
  url=$(gh issue create --repo "$GH_REPO" --title "$TITLE" --label "$LABEL" --body "$body")
  echo "Created $url"
fi
