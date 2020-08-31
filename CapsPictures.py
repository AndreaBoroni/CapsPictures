import pygame
import numpy as np
from PIL import Image
from pygame import Rect

def get_centers(img_width = 0, img_height = 0, radius = 0):
    centers = []

    delta_vertical   = np.ceil(np.sqrt(2) * radius)
    delta_horizontal = radius * 2

    current_y = radius
    indent = False

    while (current_y  - radius < img_height):
        current_x = radius
        if indent:
            current_x += radius
        while (current_x - radius < img_width):
            centers.append([current_x, current_y])
            current_x += delta_horizontal
        current_y += delta_vertical
        indent = not indent

    return centers

def get_diff(cap = None, image = None, center_x = 0, center_y = 0, radius = 0):
    
    img_w = image.shape[0]
    img_h = image.shape[1]

    s_x = int(center_x - radius)
    s_y = int(center_y - radius)
    e_x = int(center_x + radius)
    e_y = int(center_y + radius)
    
    start_x = int(max(s_x, 0))
    start_y = int(max(s_y, 0))
    end_x   = int(min(e_x, img_w))
    end_y   = int(min(e_y, img_h))

    matrix = image[start_x:end_x, start_y:end_y] # - cap[start_x - s_x:int(2 * radius - (e_x - end_x)), start_y - s_y:int(2 * radius - (e_y - end_y))]
    return np.average(matrix)

    # norm = np.linalg.norm(matrix)
    # return matrix
    # return norm

def get_images_indexes(image_data = None, caps_data = [], centers = [], radius = 0):
    
    indexes = []

    for c in centers:
        # differences = []
        # for cap in caps_data:
        #     differences.append(get_diff(cap = cap, image = image_data, center_x = c[0], center_y = c[1], radius = radius))
        # indexes.append(np.argmin(differences))
        gray_value = get_diff(cap = None, image = image_data, center_x = c[0], center_y = c[1], radius = radius)
        indexes.append(int(gray_value / 255 * 121))
    
    min_ind = np.min(indexes)
    max_ind = np.max(indexes)

    for i in range(len(indexes)):
        indexes[i] = int((indexes[i] - min_ind) / max_ind * 120)


    return indexes

pygame.init()
pygame.font.init()

caps_images = []
caps_pngs = []

r = 170
n = 46

for i in range(121):
    caps_pngs.append(pygame.image.load('C:/Users/andre/OneDrive/Desktop/Learnings/CapsPictures/Caps/Cap ' + str(i + 1) + '.png'))
    caps_pngs[i] = pygame.transform.scale(caps_pngs[i], (r, r))
    caps_images.append(pygame.surfarray.array3d(caps_pngs[i]))
    caps_images[i].swapaxes(0,1)    

Mee = pygame.image.load('C:/Users/andre/OneDrive/Desktop/Learnings/CapsPictures/Thom.jpg')
Mee = pygame.transform.scale(Mee, (n * r, int(Mee.get_height() / Mee.get_width() * n * r)))
Mee = pygame.surfarray.array3d(Mee)
Mee.swapaxes(0,1)  

shape = Mee.shape
radius = r / 2
# shape =
centers = get_centers(img_width = shape[0], img_height = shape[1], radius = radius)

indexes = get_images_indexes(image_data = Mee, caps_data = caps_images, centers = centers, radius = radius)


window = pygame.display.set_mode(Mee.shape[0:2])

while True:
    
    
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            exit()

    clock = pygame.time.Clock()

    img_w = Mee.shape[0]
    img_h = Mee.shape[1]


    for i in range(len(centers)):
        s_x = centers[i][0] - radius
        s_y = centers[i][1] - radius
        e_x = centers[i][0] + radius
        e_y = centers[i][1] + radius

        rect = Rect(int(s_x), int(s_y), int(e_x - s_x), int(e_y - s_y))
        # print(i)
        # x = s_x
        # y = s_y
        # while y < e_y:
        #     while x < e_x:
        #         np_img[x, y, :] = caps_pngs[indexes[i]][x - s_x, y - s_y, :]
        #         x += 1
        #     y += 1


        window.blit(caps_pngs[indexes[i]], rect)
    
    np_img = pygame.surfarray.array3d(window)
    new_im = Image.fromarray(np_img, 'RGB')
    new_im.save("Image_caps.png")
    
    break
    # pygame.display.update()

    # clock.tick(20)
    # pygame.time.delay(30)