import matplotlib.pyplot as plt
import numpy as np
import csv

import glob, os


def parse_flow(data_path, scale):
	#time, sent, recieved = np.loadtxt('d0p0.txt', delimiter=' ', unpack=True, skiprows=1) 
	#plt.plot(time, sent)
	with open(data_path, 'r') as data_file:
		reader = csv.reader(data_file, delimiter=' ')
		headers = next(reader)
		data = list(reader)
		data = np.array(data).astype(float)

	time = data[:, 0]
	sent = data[:, 1]
	sent[:] = [i / scale for i in sent]
	recieved = data[:, 2] 
	recieved[:] = [i / scale for i in recieved]

	return time, sent, recieved


# make sure that scale == packet size
def create_combined_plot(data_paths, scale):
	colors = "bgrcmyk"
	cur_color = 0
	plot_title = data_paths[0].split('p')[0]

	plt.xlabel('simulated time [s]')
	#plt.ylabel('bytes x' + str(scale))
	plt.ylabel('packets')
	plt.title(plot_title)
	plt.grid()

	total_sent = []
	total_recieved = []
	time_axis = []
	for data_path in data_paths:
		time, sent, recieved = parse_flow(data_path, scale)
		if len(total_sent) == 0: 
			total_sent = sent
			total_recieved = recieved
			time_axis = time
		else:
			total_sent = np.add(total_sent, sent)
			total_recieved = np.add(total_recieved, recieved)
		
		plt.plot(time, sent, color=colors[cur_color], linestyle='-', label=data_path.split('.')[0] + ' sent')
		plt.plot(time, recieved, color=colors[cur_color], linestyle=':', label=data_path.split('.')[0] + ' recv')
		cur_color = (cur_color + 1) % len(colors)
	
	total_lost = 0
	for i in range(len(total_sent)):
		total_lost = total_lost + (total_sent[i] - total_recieved[i])
	print("total lost = " + str(total_lost))
	#plt.figtext(.5, .8, "total lost = " + str(int(total_lost)))

	plt.plot(time_axis, total_sent, color=colors[cur_color], linestyle='-', label='total sent')
	plt.plot(time_axis, total_recieved, color=colors[cur_color], linestyle=':', label='total recv')	
	plt.legend()
	#plt.savefig(plot_title, bbox_inches='tight')
	plt.savefig(plot_title, bbox_inches='tight', format='eps')
	plt.show()
	plt.close()


def create_all_demands_plot(demand_paths, scale):
	# parse part
	total_sent = []
	total_recieved = []
	time_axis = []
	for data_paths in demand_paths:
		for data_path in data_paths:
			time, sent, recieved = parse_flow(data_path, scale)
			if len(total_sent) == 0: 
				total_sent = sent
				total_recieved = recieved
				time_axis = time
			else:
				total_sent = np.add(total_sent, sent)
				total_recieved = np.add(total_recieved, recieved)

	value_axis = (total_sent - total_recieved) / total_sent * 100
	for i in range(len(time_axis)):
		if total_sent[i] == 0 or total_recieved[i] == 0:
			value_axis[i] = 0

	window_size = 1000
	window_sent = []
	window_recv = []
	window_sent.append(total_sent[i])
	window_recv.append(total_recieved[i])
	for i in range(len(total_sent) - window_size - 1):
		window_sent.append(total_sent[window_size + i + 1] - total_sent[i])
		window_recv.append(total_recieved[window_size + i + 1] - total_recieved[i])
	
	window_value_axis = (np.array(window_sent) - np.array(window_recv)) / np.array(window_sent) * 100
	print(window_value_axis)
	print(time_axis[window_size:])
	
	# plot part
	plt.xlabel('simulated time [s]')
	plt.ylabel('percentage of delayed/lost packets \n 1-second window [%]')
	plt.title('all demands window')
	plt.grid()
	#plt.plot(time_axis, total_sent, linestyle='-', label='total sent')
	#plt.plot(time_axis, total_recieved, linestyle=':', label='total recv')	
	plt.gca().set_ylim([0.0, 10.0])
	plt.gca().set_xlim([0.0, 25.0])
	#plt.plot(time_axis, value_axis, linestyle='-', label='total sent-recv diff')
	plt.plot(time_axis[window_size:], window_value_axis, linestyle='-')
	plt.legend()
	plt.savefig("all_demands", bbox_inches='tight')
	plt.savefig("all_demands.eps", bbox_inches='tight', format='eps')
	plt.show()
	plt.close()
	create_all_demands_sent_recv_plot(scale, total_sent, total_recieved, time_axis)


# make sure that scale == packet size
def create_all_demands_sent_recv_plot(scale, total_sent, total_recieved, time_axis):
	# plot part
	plt.xlabel('simulated time [s]')
	#plt.ylabel('bytes x' + str(scale))
	plt.ylabel('packets')
	plt.title('all demands')
	plt.grid()
	plt.plot(time_axis, total_sent, linestyle='-', label='total sent')
	plt.plot(time_axis, total_recieved, linestyle=':', label='total recv')	
	#plt.plot(time_axis, total_sent - total_recieved, linestyle=':', label='total sent-recv diff')	

	plt.gca().set_xlim([0.0, 25.0])
	plt.legend()
	plt.savefig("all_demands_sent_recv_diff", bbox_inches='tight')
	plt.savefig("all_demands_sent_recv_diff.eps", bbox_inches='tight', format='eps')
	plt.show()
	plt.close()


# make sure that scale == packet size
def create_single_plot(data_path, scale):
	print(data_path)
	plot_title = data_path.split('.')[0]
	plt.xlabel('simulated time [s]')
	#plt.ylabel('bytes x' + str(scale))
	plt.ylabel('packets')
	plt.title(plot_title)
	plt.grid()
	time, sent, recieved = parse_flow(data_path, scale)
	plt.plot(time, sent, linestyle='-', label=data_path.split('.')[0] + ' sent')
	plt.plot(time, recieved, linestyle=':', label=data_path.split('.')[0] + ' recv')
	plt.legend()
	plt.savefig(plot_title, bbox_inches='tight')
	plt.show()
	plt.close()


def parse_queue(queue_path):
	with open(queue_path, 'r') as queue_file:
		reader = csv.reader(queue_file, delimiter=' ')
		headers = next(reader)
		data = list(reader)
		data = np.array(data).astype(float)

	time = data[:, 0]
	no_packets = data[:, 1]
	no_dropped_packets = data[:, 2]

	return time, no_packets, no_dropped_packets


def create_single_queue_plot(queue_path):
	plot_title = queue_path.split('.')[0]
	plt.xlabel('simulated time [s]')
	plt.ylabel('number of packets')
	plt.title(plot_title)
	plt.grid()
	time, no_packets, no_dropped_packets = parse_queue(queue_path)
	average_queue = np.sum(no_packets) / len(no_packets)
	plt.plot(time, no_packets, linestyle='-')
	plt.hlines(average_queue, 0, time[-1], label='average = ' + "{:.3f}".format(average_queue))
	plt.legend()
	plt.savefig(plot_title, bbox_inches='tight')
	#plt.savefig(plot_title, bbox_inches='tight', format='eps')
	plt.close()


def create_combined_queue_plot(data_paths, queue_size):
	total_queue = []
	total_dropped = []
	time_axis = []
	for data_path in data_paths:
		time, queue, dropped = parse_queue(data_path)
		if len(total_queue) == 0: 
			total_queue = queue
			total_dropped = dropped
			time_axis = time
		else:
			total_queue = np.add(total_queue, queue)
			total_dropped = np.add(total_dropped, dropped)
	
	number_of_queues = len(data_paths)
	total_queue = [i / (number_of_queues * queue_size) * 100 for i in total_queue]
	average_queue = np.sum(total_queue) / len(total_queue)
	print(average_queue)
	
	# average queue accupancy plot 
	plt.xlabel('simulated time [s]')
	plt.ylabel('average queue occupancy [%]')
	plt.title("queue occupancy")
	plt.grid()
	plt.plot(time_axis, total_queue, linestyle='-')
	plt.hlines(average_queue, 0, time_axis[-1], label='average = ' + "{:.3f}".format(average_queue))
	plt.legend()
	plt.savefig("queue_occupancy", bbox_inches='tight')
	plt.savefig("queue_occupancy.eps", bbox_inches='tight', format='eps')
	plt.show()
	plt.close()
	# total no dropped packets plot
	plt.xlabel('simulated time [s]')
	plt.ylabel('total dropped packets')
	plt.title("dropped packets")
	plt.grid()
	plt.plot(time_axis, total_dropped, linestyle='-')
	plt.legend()
	plt.savefig("queue_total_dropped", bbox_inches='tight')
	plt.savefig("queue_total_dropped.eps", bbox_inches='tight', format='eps')
	plt.show()
	plt.close()


def get_all_demand_paths_in_dir(test_path, D):
	os.chdir(test_path)
	res = []
	for d in range(D):
		res.append([path for path in glob.glob("d" + str(d) + "p*.txt") if path[0] == 'd'])
	return res


def get_all_queue_paths_in_dir(test_path):
	os.chdir(test_path)
	return [path for path in glob.glob("*.txt") if path[0] == 'e']


###############
D = 132
test_path = "/home/ilya/Desktop/simdata/rep_poisson/DL-quad-mid-simple-alpha05-beta1-fix/ftest4/res"
#test_path = "/home/ilya/Desktop/simdata/n2e1d1/res"
demand_paths = get_all_demand_paths_in_dir(test_path, D)
queue_paths = get_all_queue_paths_in_dir(test_path)

scale = 1250
create_all_demands_plot(demand_paths, scale)
create_combined_plot(demand_paths[0], scale)
#for demand_path in demand_paths:
#	create_single_plot(demand_path, scale)

queue_size = 1000
create_combined_queue_plot(queue_paths, queue_size)
for queue_path in queue_paths:
	create_single_queue_plot(queue_path)