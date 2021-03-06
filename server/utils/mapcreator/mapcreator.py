from tkinter import *
import math
import pickle
from PIL import ImageTk, Image
from utils.mapcreator import map
from utils import coordinate


class MapCreator:

    def __init__(self):

        self.n1 = []
        self.n2 = []
        self.img_h = 0
        self.map = map.Map()

        scale = 0.9

        root = Tk()
        root.state("zoomed")

        f1 = Frame(root)
        f1.grid(row=1, column=1)
        f4 = Frame(root)
        f4.grid(row=1, column=2)

        w = Canvas(f1, width=int(root.winfo_screenwidth()*scale), height=int(root.winfo_screenheight()*scale))
        w.pack()

        img = Image.open("map.png")
        ratio = min(int(w.cget("width"))/img.size[0], int(w.cget("height"))/img.size[1])
        size = int(img.size[0]*ratio), int(img.size[1]*ratio)
        self.img_h = size[1]
        img = img.resize(size, Image.ANTIALIAS)
        img2 = ImageTk.PhotoImage(img)
        w.create_image(img.size[0]/2, img.size[1]/2, image=img2)

        v1 = StringVar()
        v2 = StringVar()

        self.nd1 = None
        self.nd2 = None

        def add_node(eventorigin):
            global x, y
            x = eventorigin.x
            y = eventorigin.y
            w.create_oval(x - 4, y - 4, x + 4, y + 4, fill="black", tags="node")

        def create_node(event):
            if len(self.n1) != 0 and len(self.n2) != 0:
                w.create_line(self.n1[0]*ratio, self.img_h-self.n1[1]*ratio,
                              self.n2[0]*ratio, self.img_h-self.n2[1]*ratio, fill="black")
                node1 = coordinate.Coordinate(self.n1[0], self.n1[1])
                node2 = coordinate.Coordinate(self.n2[0], self.n2[1])
                self.map.add_node(node1, node2)
                self.n1 = []
                self.n2 = []
                v1.set("")
                v2.set("")
                w.itemconfig(self.nd1, fill="black")
                w.itemconfig(self.nd2, fill="black")
                self.nd1 = None
                self.nd2 = None

        def one_way_nodes(event):
            if len(self.n1) != 0 and len(self.n2) != 0:
                w.create_line(self.n1[0]*ratio, self.img_h-self.n1[1]*ratio,
                              self.n2[0]*ratio, self.img_h-self.n2[1]*ratio, fill="black")
                node1 = coordinate.Coordinate(self.n1[0], self.n1[1])
                node2 = coordinate.Coordinate(self.n2[0], self.n2[1])
                self.map.add_one_way_node(node1, node2)
                self.n1 = []
                self.n2 = []
                v1.set("")
                v2.set("")
                w.itemconfig(self.nd1, fill="black")
                w.itemconfig(self.nd2, fill="black")
                self.nd1 = None
                self.nd2 = None

        def clear_nodes(event):
            self.n1 = []
            self.n2 = []
            v1.set("")
            v2.set("")
            if self.nd2:
                w.itemconfig(self.nd1, fill="black")
                w.itemconfig(self.nd2, fill="black")

                self.nd2 = None
            elif self.nd1:
                w.itemconfig(self.nd1, fill="black")
                self.nd1 = None

        def exit_program(event):
            with open('map.txt', 'wb') as outfile:
                pickle.dump(self.map.nodes, outfile)
            outfile.close()
            root.quit()

        def load_map():
            try:
                with open('map.txt', 'rb') as infile:
                    data = pickle.load(infile)
                infile.close()
                self.map.nodes = data
                for key, value in self.map.nodes.items():
                    for key2, value2 in value.items():
                        w.create_line(key.x*ratio, self.img_h - key.y*ratio, key2.x*ratio,
                                      self.img_h - key2.y*ratio, fill="black")
                    w.create_oval(key.x*ratio - 4, self.img_h - key.y*ratio - 4,
                                  key.x*ratio + 4, self.img_h - key.y*ratio + 4, fill="black", tags="node")
            except EOFError:
                print("No map loaded")

        def buttons():
            f = Frame(f4)
            f.grid(row=2, column=1, sticky="w")
            b1 = Button(f, text="add nodes")
            b1.bind("<Button-1>", create_node)
            b1.grid(row=1, sticky="w")
            b2 = Button(f, text="clear nodes")
            b2.bind("<Button-1>", clear_nodes)
            b2.grid(row=2, sticky="w")
            b3 = Button(f, text="save and quit")
            b3.bind("<Button-1>", exit_program)
            b3.grid(row=3, sticky="w")
            b4 = Button(f, text="one way nodes")
            b4.bind("<Button-1>", one_way_nodes)
            b4.grid(row=4, sticky="w")
            w.tag_bind('node', '<Button-1>', on_object_click)
            w.bind('<Button-3>', add_node)

        def on_object_click(event):
            item = w.find_closest(event.x, event.y)
            if 'node' in w.gettags(item) and not self.nd2:
                n = w.coords(item)
                w.itemconfig(item, fill="green")
                o = int((n[0] + n[2]) / 2/ratio), int(math.fabs((n[1] + n[3]) / 2 - self.img_h)/ratio)
                print(o)
                if len(self.n1) == 0:
                    self.n1 = o
                    self.nd1 = item
                    v1.set(str(self.n1))
                elif len(self.n2) == 0 and o != self.n1:
                    self.n2 = o
                    self.nd2 = item
                    v2.set(str(self.n2))

        def labels():
            f3 = Frame(f4)
            f3.grid(row=1, column=1, sticky="w")
            entry_n1 = Label(f3, textvariable=v1)
            entry_n2 = Label(f3, textvariable=v2)
            label1 = Label(f3, text="Node 1: ")
            label2 = Label(f3, text="Node 2: ")
            label1.grid(column=1, row=1, sticky="w")
            entry_n1.grid(column=2, row=1, sticky="w")
            label2.grid(column=1, row=2, sticky="w")
            entry_n2.grid(column=2, row=2, sticky="w")

        buttons()
        labels()
        load_map()
        root.mainloop()


if __name__ == "__main__":
    m = MapCreator()




