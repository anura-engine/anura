//Generate the data block for the math easing function unit tests. Some massaging will is needed.
//Run in debug console on http://easings.net/.

var out = "\n[\n";
[
	'easeInBack',
	'easeInBounce',
	'easeInCirc',
	'easeInCubic',
	'easeInElastic',
	'easeInExpo',
	'easeInOutBack',
	'easeInOutBounce',
	'easeInOutCirc',
	'easeInOutCubic',
	'easeInOutElastic',
	'easeInOutExpo',
	'easeInOutQuad',
	'easeInOutQuart',
	'easeInOutQuint',
	'easeInOutSine',
	'easeInQuad',
	'easeInQuart',
	'easeInQuint',
	'easeInSine',
	'easeOutBack',
	'easeOutBounce',
	'easeOutCirc',
	'easeOutCubic',
	'easeOutElastic',
	'easeOutExpo',
	'easeOutQuad',
	'easeOutQuart',
	'easeOutQuint',
	'easeOutSine',
].forEach(function(ease) {
	out += "	[";
	for (var i = 0; i <= 10; i++) {
		var easeResult = jQuery.easing[ease]($, i/10, 0, 1, 1);
		out += ((easeResult<0?'':' ') + Math.round(easeResult*10000)/10000 + '000000').slice(0,7) + ', ';
	}
	out += "] //" + ease+ ",\n";
});
out += "]\n";
console.log(out);