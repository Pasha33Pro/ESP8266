import matplotlib.pyplot as plt
from tkinter import Tk, filedialog

def read_data(filename):
    data = {'High': [], 'Medi': [], 'Low': []}
    with open(filename, 'r') as file:
        for line in file:
            parts = line.strip().split()
            for part in parts:
                key, value = part.split(':')
                data[key].append(int(value))
    return data

def plot_data(data):
    plt.figure(figsize=(10, 6))

    plt.plot(data['High'], label='High', marker='o', linestyle='-', color='red')
    plt.plot(data['Medi'], label='Medi', marker='s', linestyle='--', color='indianred')
    plt.plot(data['Low'], label='Low', marker='^', linestyle='-.', color='orange')

    plt.title('Визуализация Log-ов', fontsize=16)
    plt.xlabel('Шаг', fontsize=12)
    plt.ylabel('Значение', fontsize=12)

    plt.legend()

    plt.grid(True)

    plt.tight_layout()
    plt.show()

def select_file():
    root = Tk()
    root.withdraw()
    file_path = filedialog.askopenfilename(
        title="Выберите файл log.txt",
        filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
    )
    return file_path

if __name__ == "__main__":
    filename = select_file()

    if not filename:
        print("No file selected. Exiting...")
    else:
        data = read_data(filename)

        plot_data(data)