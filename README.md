Once up on a time in contributed code to xchat - later on Zed decided to make a shareware version of it available to windows users.

I said that this didn't comply with the GPLv2 or later license that I had contributed under, to which he replied "You haven't told me this" or something like that.

The odd thing is, GPLv2 work + modification => patch to author => NOT GPL? If only large companies knew... ;)

Anyway, I decided to fork xchat - I had some friends that helped me out some but eventually I ran in to trouble with GTK and the merges increased in size, eventually it all died out. However, we did make some improvements which is why I upload this, here, today.

Improvements we made:
* Rewrote the entire parser
* Handled the 005 numeric (FEATURE) properly including parsing of all the data
* Throttling that took lag in to account and that had:
  - Prioritized queues in the order: server, channel modes, pmsg, notice
  - Queues followed nick changes
  - Queues merged modes to push less lines
* CTCP v2 compliance
* And more things - Look at the code, remind me... =)

To me this was a fun little project, we used splay and leaky-buckets all over the place.

I have been toying with the idea of porting alot of this to irssi but it hasn't happend so far.

Anyway, please have a look - I will try to update this page with more information on where the things are located and so forth.
