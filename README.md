Once up on a time in contributed code to xchat - later on Zed decided to make a shareware version of it available to windows users.

I said that this didn't comply with the GPLv2 or later license that I had contributed under, to which he replied "You haven't told me this" or something like that.

The odd thing is, GPLv2 work + modification => patch to author => NOT GPL? If only large companies knew... ;)

Anyway, I decided to fork xchat - I had some friends that helped me out some but eventually I ran in to trouble with GTK and the merges increased in size, eventually it all died out. However, we did make some improvements which is why I upload this, here, today.

Improvements we made:
* Rewrote the entire parser
	https://github.com/pomac303/attic-rage/commit/8169695054ee789a16299c7123e22f471c655449
	https://github.com/pomac303/attic-rage/commit/76a501401a8d9a73cf09ee4f5f632c4e641a29e9
	https://github.com/pomac303/attic-rage/commit/c91325413d2f088292c4516c47ee8dc90275d971
* Capabilities handling
	https://github.com/pomac303/attic-rage/commit/f8fba69ea39d87de9d07165301e7478c155401b6
	https://github.com/pomac303/attic-rage/commit/fb9c94fbce1b919ddc4f304400419ec1e860375f
* Use CPRIVMSG and CNOTICE automatically when possible
	https://github.com/pomac303/attic-rage/commit/279f2a11756a595907c51f81062ae37c2597a6c6
* Handled the 005 numeric (FEATURE) properly including parsing of all the data
* Throttling that took lag in to account and that had:
  - Prioritized queues in the order: command, mode, message and reply
  	https://github.com/pomac303/attic-rage/commit/4ffcd7eaf7d4341d2abaf8095710502fe8f689e4
  - Queues followed nick changes
  - Queues merged modes to push less lines
  	https://github.com/pomac303/attic-rage/commit/e49d66ccc9fac2918d4447333f8d2f0deb332886
* CTCP v2 compliance
* And more things - Look at the code, remind me... =)

To me this was a fun little project, we used splay and leaky-buckets all over the place.

I have been toying with the idea of porting a lot of this to irssi but it hasn't happend so far.

Anyway, please have a look - I will try to update this page with more information on where the things are located and so forth.
