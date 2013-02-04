# How to contribute

## File naming conventions

For C++:

* Templated functions and classes in file ending with the ".hpp"
  extension
* Functions and classes declaration in file ending with the ".h"
  extension
* Function and classes definitions in file ending with the ".cpp"
  extension

## Adding a plugin

Create a file named irony-PLUGIN_NAME.el

This file should contain 2 methods takind no argument:

1. `irony-PLUGIN_NAME-enable` called by `irony-enable module-names`
   usually in user configuration file. You can add a hook for on
   irony-mode if you need to be active in each irony-mode buffer.

2. `irony-PLUGIN_NAME-enable` called by `irony-disable module-names`
   usually in user configuration file. If a hook was added during
   `irony-PLUGIN_NAME-enable` you can remove it inside this function.


Example from the file *elisp/plugins/irony-ac.el*:

~~~~~ el
(defun irony-ac-setup ()
  "Hook to run for `auto-complete-mode' when `irony-mode' is
activated."
  (interactive)
  (add-to-list 'ac-sources 'ac-source-irony)
  (define-key irony-mode-map [(control return)] 'ac-complete-irony))

(defun irony-ac-enable ()
  "Enable `auto-complete-mode' handling of `irony-mode'
completion results."
  (add-hook 'irony-mode-hook 'irony-ac-setup))

(defun irony-ac-disable ()
  "Disable `auto-complete-mode' handling of `irony-mode'
completion results."
  (remove-hook 'irony-mode-hook 'irony-ac-setup))
~~~~~

For more information look at the existing plugins or ask me.