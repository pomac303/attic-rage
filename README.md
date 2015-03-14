Rage
====

History
-------

A long time ago I contributed some code to X-Chat.

* 2.1.0
	- Tab nick completion fixes: Crash with space-tab and glib critical warning (Ian Kumlien).
* 2.0.10
	- Brand-new tab completion code (Ian Kumlien). This also fixes: Tab expansion not working behind umlauts [956127].
* 2.0.0pre1:
	- Don't allow the Perl plugin to be loaded twice (Ian Kumlien).

Later on Peter 'Zed' Zelezny  decided to make a shareware (binary only) version of it available to windows users.

If you don't remember: http://slashdot.org/story/04/08/30/1859210/does-shareware-x-chat-for-windows-violate-the-gpl
And: http://xchatdata.net/Using/SharewareBackground

I am one of the people who said that my work, as in patch, was a derivative work of the original.
The original was licensed to me by Zed under GPLv2 or newer. Thus the work I sent would be GPLv2.

He stated that since I did not explicitly state that this was the case, it simply wasn't.

This is when I decided to fork X-Chat, we decided to call the fork Rage.

And this is the historical subversion repository of a major rewrite that never finished.

Why did it die?
---------------

The merges with the main X-Chat branch became to heavy and I ran in to real showstoppers
where GTK didn't want me to change channel modes in the UI (based on the 005 reply).

This is where time and energy just wasn't enough.

So what did you do?
-------------------

Improvements we made:
* Rewrote the entire parser
  1. https://github.com/pomac303/attic-rage/commit/8169695054ee789a16299c7123e22f471c655449
  2. https://github.com/pomac303/attic-rage/commit/76a501401a8d9a73cf09ee4f5f632c4e641a29e9
  3. https://github.com/pomac303/attic-rage/commit/c91325413d2f088292c4516c47ee8dc90275d971
* Capabilities handling
  1. https://github.com/pomac303/attic-rage/commit/f8fba69ea39d87de9d07165301e7478c155401b6
  2. https://github.com/pomac303/attic-rage/commit/fb9c94fbce1b919ddc4f304400419ec1e860375f
* Use CPRIVMSG and CNOTICE automatically when possible
  1. https://github.com/pomac303/attic-rage/commit/279f2a11756a595907c51f81062ae37c2597a6c6
* Handled the 005 numeric (FEATURE) properly including parsing of all the data
* Throttling that took lag in to account and that had:
  - Prioritized queues in the order: command, mode, message and reply
    1. https://github.com/pomac303/attic-rage/commit/4ffcd7eaf7d4341d2abaf8095710502fe8f689e4
  - Queues followed nick changes
  - Queues merged modes to push less lines
    1. https://github.com/pomac303/attic-rage/commit/e49d66ccc9fac2918d4447333f8d2f0deb332886
* CTCP v2 compliance
* And more things - Look at the code, remind me... =)

What could happen now?
----------------------

I have been toying with the idea of porting a lot of this to irssi but it hasn't happend so far.

