<h1>Steps of {{app_name}}</h1>
%for step in steps:
%    if step[0] == 'free':
<p>{{step[0]}} {{step[1]}}</p>
%    else:
<p>{{step[0]}} {{step[1]}} {{step[2]}} </p>
%end
%end
