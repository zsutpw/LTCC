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


def create_combined_plot(data_paths, scale):
	colors = "bgrcmyk"
	cur_color = 0
	plot_title = data_paths[0].split('p')[0]

	plt.xlabel('simulation time [s]')
	plt.ylabel('bytes x' + str(scale))
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


def create_single_plot(data_path, scale):
	plot_title = data_path.split('.')[0]
	plt.xlabel('simulation time [s]')
	plt.ylabel('bytes x' + str(scale))
	plt.title(plot_title)
	plt.grid()
	time, sent, recieved = parse_flow(data_path, scale)
	plt.plot(time, sent, linestyle='-', label=data_path.split('.')[0] + ' sent')
	plt.plot(time, recieved, linestyle=':', label=data_path.split('.')[0] + ' recv')
	plt.legend()
	plt.savefig(plot_title, bbox_inches='tight')
	plt.close()


def parse_queue(queue_path):
	with open(queue_path, 'r') as queue_file:
		reader = csv.reader(queue_file, delimiter=' ')
		headers = next(reader)
		data = list(reader)
		data = np.array(data).astype(float)

	time = data[:, 0]
	no_packets = data[:, 1]

	return time, no_packets


def create_single_queue_plot(queue_path):
	plot_title = queue_path.split('.')[0]
	plt.xlabel('simulation time [s]')
	plt.ylabel('number of packets')
	plt.title(plot_title)
	plt.grid()
	time, no_packets = parse_queue(queue_path)
	plt.plot(time, no_packets, linestyle='-')
	#plt.legend()
	plt.savefig(plot_title, bbox_inches='tight')
	#plt.savefig(plot_title, bbox_inches='tight', format='eps')
	plt.close()


def create_combined_queue_plot(data_paths, queue_size):
	plot_title = "queue occupancy"

	plt.xlabel('simulation time [s]')
	plt.ylabel('average queue occupancy [%]')
	plt.title(plot_title)
	plt.grid()

	total_queue = []
	time_axis = []
	for data_path in data_paths:
		time, queue = parse_queue(data_path)
		if len(total_queue) == 0: 
			total_queue = queue
			time_axis = time
		else:
			total_queue = np.add(total_queue, queue)
	
	number_of_queues = len(data_paths)
	total_queue = [i / (number_of_queues * queue_size) * 100 for i in total_queue]	
	
	plt.plot(time_axis, total_queue, linestyle='-')
	plt.legend()
	#plt.savefig(plot_title, bbox_inches='tight')
	plt.savefig(plot_title, bbox_inches='tight', format='eps')
	plt.show()
	plt.close()


def get_all_demand_paths_in_dir(test_path):
	os.chdir(test_path)
	return [path for path in glob.glob("*.txt") if path[0] == 'd']


def get_all_queue_paths_in_dir(test_path):
	os.chdir(test_path)
	return [path for path in glob.glob("*.txt") if path[0] == 'e']


###############
test_path = "/home/ilya/Desktop/simdata/rep/GAFT-p-no-demand-cutting/test4/res"
demand_paths = get_all_demand_paths_in_dir(test_path)
queue_paths = get_all_queue_paths_in_dir(test_path)

scale = 1000
#create_combined_plot(demand_paths, scale)
#for demand_path in demand_paths:
#	create_single_plot(demand_path, scale)

queue_size = 1000
create_combined_queue_plot(queue_paths, queue_size)
for queue_path in queue_paths:
	create_single_queue_plot(queue_path)