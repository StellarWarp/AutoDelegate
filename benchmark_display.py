import json
import os
import subprocess
import matplotlib.pyplot as plt
import sys

# 运行Google Benchmark并捕获输出
def run_benchmark(executable, options):
    if sys.platform.startswith('linux'):
        command = f"{executable} {options}"
    else:
        command = f"{executable} {options}"
    
    process = subprocess.run('xmake f -m release -a x64 -y', shell=True, text=True)
    print(process.stdout)
    process = subprocess.run('xmake build DelegateBenchmark', shell=True, text=True)
    if process.returncode != 0:
        print(process.stderr)
        exit(1)
    process = subprocess.run(command, shell=True, text=True)
    if process.returncode == 0:
        print("Subprocess executed successfully")
    else:
        raise Exception(f"Subprocess failed with exit code {process.returncode}")



# 主函数
def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    print(os.getcwd())
    benchmark_file = 'results.json'
    # if is windows
    if sys.platform.startswith('win'):
        executable = os.getcwd() + r"/build/windows/x64/release/DelegateBenchmark.exe"
    elif sys.platform.startswith('linux'):
        executable = os.getcwd() + r"/build/linux/x64/release/DelegateBenchmark"
    options = f"--benchmark_repetitions=4 --benchmark_format=json --benchmark_out={benchmark_file}"

    #check exist for executable
    if not os.path.exists(executable):
        print(f"Executable {executable} not found")
        exit(1)

    try:
        run_benchmark(executable, options)
    except Exception as e:
        print(e)
        exit(1)


    with open(benchmark_file, 'r') as file:
        data = json.load(file)

        res = {}
        for benchmark in data['benchmarks']:
            name = benchmark['name'].split('/')[0]
            if benchmark['run_type'] == 'iteration':
                if name not in res:
                    res.setdefault(name,
                                     {'cpu_time': [],
                                      'real_time': [],
                                    'iterations': [],
                                    'mean': 0,
                                    'median': 0,
                                    'stddev': 0,
                                    'cv': 0
                                       })

                res[name]['cpu_time'].append(benchmark['cpu_time'])
                res[name]['real_time'].append(benchmark['real_time'])
                res[name]['iterations'].append(benchmark['iterations'])
            elif benchmark['run_type'] == 'aggregate':
                match benchmark['aggregate_name']:
                    case 'mean':
                        res[name]['mean'] = benchmark['cpu_time']
                    case 'median':
                        res[name]['median'] = benchmark['cpu_time']
                    case 'stddev':
                        res[name]['stddev'] = benchmark['cpu_time']
                    case 'cv':
                        res[name]['cv'] = benchmark['cpu_time']

        names = list(res.keys())
        means = [res[name]['mean'] for name in names]
        meidans = [res[name]['median'] for name in names]
        point_x = []
        point_values = []
        for i,name in enumerate(names):
            for time in res[name]['cpu_time']:
                point_x.append(i)
                point_values.append(time)

        stddevs = [res[name]['stddev'] for name in names]
        colors = [
            'green' if 'Virtual' in name else 'blue'
            for name in names
        ]
        names = ['\n'.join(name.split('_')[1:]) for name in names]

        plt.bar(names, means, yerr=stddevs, color=colors, alpha=0.2, error_kw=dict(lw=1, capsize=5, capthick=1))
        plt.scatter(point_x, point_values, color='cyan', alpha=0.1)
        plt.scatter(names, meidans, color='green', alpha=0.5)

        # plt.xlabel('Benchmark Name')
        plt.ylabel('CPU Time')
        plt.title('Benchmark Performance')
        plt.xticks(rotation=80, ha='right')
        plt.tight_layout()
        plt.savefig('benchmark_fig')
        plt.show()


if __name__ == '__main__':
    main()
