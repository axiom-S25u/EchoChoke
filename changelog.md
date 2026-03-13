## 1.1.4
- forgot the filter, webhooks that were added in the last upd wont work.
## 1.1.2
* fixed bugs

## 1.1.0
* added per-level minimum percent via `id:percent` format in whitelist (e.g. `34057654:40,83914474:57.41`)
* decimal percent support — deaths now show as `67.41%` instead of just `67%`
* added level blacklist to hide upcoming projects from the webhook
* platformer roast support — roasts based on session time and total grind time instead of percent
* fixed congrats messages: `()` is now the username and `[]` is the level name (was backwards)
* attempts now shows total level attempts instead of session attempts
* added `##` token for session time in custom messages
* added a ton more roast, victory, and platformer roast messages

## 1.0.12
* track consecutive deaths at the exact same %
* if you die at the same % more than 5 times, send a "stuck" message to the webhook (appends to roast on new best, otherwise sends a text-only message)
* added setting: Enable Stuck Messages
* windows build fix: add /FS to prevent PDB C1041 errors

## 1.0.11
* added github and discord links

## 1.0.9
* added more roasts
* added a disable roasts option
* added variables like `()`, `[]`, `{}`, `<>`, `!!`, and `~~`
* added custom message formatting
* meow

## 1.0.8
* added startpos detection

## 1.0.7
* added practice detection

## 1.0.5
* fixed webhook stuff

## 1.0.1
* added settings

## 1.0.0
* basically just made it send the images to a webhook
